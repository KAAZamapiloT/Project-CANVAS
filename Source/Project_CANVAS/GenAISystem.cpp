// Fill out your copyright notice in the Description page of Project Settings.


#include "GenAISystem.h"
#include "SceneStateTracker.h"
#include "JsonParser.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "AssetIndexer.h"
#include "AssetTypeCategories.h"

#include "MeshResolverLLM.h"
#include "TextureResolverLLM.h"
#include "ToolContextInterfaces.h"


void UGenAISystem::RequestSceneChange(FString UserPrompt,UWorld* WorldContext,USceneHistoryManager* HistoryManager)
{
	

	// 1. Get the GameInstance and AssetIndexer
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContext);
	USceneStateTracker* Tracker = GameInstance->GetSubsystem<USceneStateTracker>();
    if (!GameInstance || !Tracker->AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("GenAISystem: Cannot find GameInstance or AssetIndexer!"));
        return;
    }

    // 2. Check if the asset list is ready
    if (!Tracker->AssetIndexer->IsScanComplete())
    {
        UE_LOG(LogTemp, Warning, TEXT("GenAISystem: Asset scan is not complete. Please wait."));
        return;
    }

    
	LastUserPrompt = UserPrompt;
	CachedWorld = WorldContext;
	CachedHistory = HistoryManager;

// RESET
	bIsMeshReady = false;
	
	bIsTexReady = false;
	
	DraftMeshJson = "";
	DraftTexJson = "";
	PrunedMeshList = "";
	PrunedTextureList = "";

	MeshL->RequestPlan(UserPrompt,WorldContext,HistoryManager);
	TexL->RequestPlan(UserPrompt,WorldContext,HistoryManager);

	
	
	
	
}

void UGenAISystem::OnLLMResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Ollama request failed!"));
        return;
    }
    
	FString ResponseString = Response->GetContentAsString();
	UE_LOG(LogTemp, Warning, TEXT("GenAI: RAW GROQ RESPONSE:\n%s"), *ResponseString);
    
	TSharedPtr<FJsonObject> GroqJsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
    
	if (FJsonSerializer::Deserialize(Reader, GroqJsonObject) && GroqJsonObject.IsValid())
	{
		FString LlmResponseString;
        
		// GEMINI PARSING LOGIC
		const TArray<TSharedPtr<FJsonValue>>* Candidates;
		if (GroqJsonObject->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
		{
			TSharedPtr<FJsonObject> ContentObj = (*Candidates)[0]->AsObject()->GetObjectField(TEXT("content"));
			const TArray<TSharedPtr<FJsonValue>>* Parts;
			if (ContentObj->TryGetArrayField(TEXT("parts"), Parts) && Parts->Num() > 0)
			{
				LlmResponseString = (*Parts)[0]->AsObject()->GetStringField(TEXT("text"));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("GenAI: No candidates found"));
			return;
		}
		// === END OF GROQ-SPECIFIC CODE ===
        
		UE_LOG(LogTemp, Log, TEXT("GenAI: Extracted LLM response (raw): %s"), *LlmResponseString);
        
        // === STEP 1: Extract JSON between first { and last } ===
        int32 JsonStart = -1;
        int32 JsonEnd = -1;
        
        if (LlmResponseString.FindChar(TEXT('{'), JsonStart) && 
            LlmResponseString.FindLastChar(TEXT('}'), JsonEnd) && 
            JsonStart < JsonEnd)
        {
            LlmResponseString = LlmResponseString.Mid(JsonStart, (JsonEnd - JsonStart) + 1);
            UE_LOG(LogTemp, Log, TEXT("Step 1 - Extracted JSON block"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("JSON parsing error: Could not find valid braces."));
            LlmResponseString = "";
        }
        
        // === STEP 2: Remove JSON comments (// ...) ===
        if (!LlmResponseString.IsEmpty())
        {
            TArray<FString> Lines;
            LlmResponseString.ParseIntoArrayLines(Lines);
            FString CleanedJSON;
            
            for (const FString& Line : Lines)
            {
                FString ProcessedLine = Line;
                
                // Find and remove single-line comments
                int32 CommentIndex = ProcessedLine.Find(TEXT("//"));
                if (CommentIndex != INDEX_NONE)
                {
                    ProcessedLine = ProcessedLine.Left(CommentIndex);
                }
                
                // Trim and keep non-empty lines
                ProcessedLine = ProcessedLine.TrimStartAndEnd();
                if (!ProcessedLine.IsEmpty())
                {
                    CleanedJSON += ProcessedLine + TEXT("\n");
                }
            }
            
            LlmResponseString = CleanedJSON.TrimStartAndEnd();
            UE_LOG(LogTemp, Log, TEXT("Step 2 - Removed comments"));
        }
        
        // === STEP 3: Final log and parse ===
        UE_LOG(LogTemp, Warning, TEXT("Cleaned LLM response for parser:\n%s"), *LlmResponseString);
        
		// 4. Parse the Director's Plan
		FEnhancedScenePlan MasterPlan = UJsonParser::CreatePlan(LlmResponseString);

		// =========================================================
		// 🛡️ FALLBACK LOGIC (The Fix)
		// =========================================================
    
		// Logic: If the Master returned 0 spawns, but we KNOW the Mesh Agent found some...
		// Then the Master failed (hallucinated "nothing to do").
		bool bMasterFailed = (MasterPlan.SpawnRequest.Num() == 0 && !DraftMeshJson.IsEmpty() && DraftMeshJson.Len() > 10);

		if (bMasterFailed)
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ GenAI: Master returned EMPTY spawns! Reverting to Draft Plans..."));
			ExecuteFallbackPlan();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("✅ GenAI: Master Success! Spawning %d items."), MasterPlan.SpawnRequest.Num());
			OnThemeDataReady.Broadcast(MasterPlan, LastUserPrompt);
		}
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse Ollama's MAIN response: %s"), *ResponseString);
    }

	
}

void UGenAISystem::AttemptSynthesis(FString UserPrompt, UWorld* WorldContext, USceneHistoryManager* HistoryManager)
{
	// 1. Safety Check: Gates
	if (!bIsMeshReady)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ AttemptSynthesis Aborted: MESH FALSE!"));
		return;
	}
	if (!bIsTexReady)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ AttemptSynthesis Aborted: TEXTURE FALSE!"));
		return;
	}

	// 🆕 2. CHECK THE LATCH (Prevent Double-Firing)
	if (bHasSynthesized) 
	{
		UE_LOG(LogTemp, Warning, TEXT("🛑 GenAI: Synthesis blocked (Already running for this request)."));
		return;
	}

	// 🆕 3. LOCK THE LATCH
	bHasSynthesized = true;
	// 2. Safety Check: World Context
	// If the passed world is null, try to fall back to the System's world
	UWorld* SafeWorld = WorldContext;
	if (!SafeWorld)
	{
		SafeWorld = GetWorld();
	}

	if (!SafeWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ AttemptSynthesis Aborted: No valid World Context!"));
		return;
	}

	// 3. Safety Check: Subsystems
	UGameInstance* GI = UGameplayStatics::GetGameInstance(SafeWorld);
	if (!GI) return;

	USceneStateTracker* Tracker = GI->GetSubsystem<USceneStateTracker>();
	if (!Tracker || !Tracker->AssetIndexer)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ AttemptSynthesis Aborted: AssetIndexer not found!"));
		return;
	}

	// 4. Proceed
	FString MasterPrompt = ConstructMasterPrompt(UserPrompt, Tracker->AssetIndexer);
    // === CHANGES START HERE ===
    
    // Get API key
	API_KEY APIKey;
	FString GeminiKey = APIKey.GetGeminiKey();// <--- Gemini Key

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    
	// Gemini URL
	FString Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%s"), *GeminiKey);
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Gemini Payload
	FString Payload = FString::Printf(TEXT(
	   "{"
	   "  \"contents\": [{"
	   "    \"parts\": [{"
	   "      \"text\": \"%s\""
	   "    }]"
	   "  }],"
	   "  \"generationConfig\": {"
	   "    \"temperature\": 0.1,"
	   "    \"responseMimeType\": \"application/json\""
	   "  }"
	   "}"
	), *MasterPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

	Request->SetContentAsString(Payload);
	Request->OnProcessRequestComplete().BindUObject(this, &UGenAISystem::OnLLMResponseReceived);
	Request->ProcessRequest();
}



// === WATERFALL CHAIN CALLBACKS ===

void UGenAISystem::OnMeshPlanReady(FString Plan, FString Choices)
{
	DraftMeshJson = Plan;
	PrunedMeshList = Choices;
	bIsMeshReady = true;
    
    if (bIsTexReady)
    {
    	if (CachedWorld.IsValid())
    	{
    		AttemptSynthesis(LastUserPrompt,CachedWorld.Get(), CachedHistory);return;
    	}
	
    
    	UWorld* World = GetWorld();
    	if (World)
    	{
    		AttemptSynthesis(LastUserPrompt, World, CachedHistory);
    		return;
    	}
    }
    	
}



void UGenAISystem::OnTexturePlanReady(FString TexturePlan, FString Choices)
{
	DraftTexJson = TexturePlan;
	PrunedTextureList = Choices;
	bIsTexReady = true;
	
	if (bIsMeshReady)
	{
		if (CachedWorld.IsValid())
		{
			AttemptSynthesis(LastUserPrompt,CachedWorld.Get(), CachedHistory);return;
		
		}
	
    
		UWorld* World = GetWorld();
		if (World)
		{
			AttemptSynthesis(LastUserPrompt, World, CachedHistory);
			return;
		}
	}
}

void UGenAISystem::Initialize()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	if (!GI) return;
	MeshL=UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<UMeshResolverLLM>();
	
	TexL= UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<UTextureResolverLLM>();

	MeshL->OnMeshPlanReady.AddDynamic(this,&UGenAISystem::OnMeshPlanReady);

	TexL->OnTexturePlanReady.AddDynamic(this,&UGenAISystem::OnTexturePlanReady);

}

void UGenAISystem::Deinitialize()
{
	//MeshL=UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<UMeshResolverLLM>();
	//LightL=UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<ULightingResolverLLM>();
	//TexL= UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<UTextureResolverLLM>();
	UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	if (!GI) return;
	MeshL->OnMeshPlanReady.RemoveDynamic(this,&UGenAISystem::OnMeshPlanReady);
	
	TexL->OnTexturePlanReady.RemoveDynamic(this,&UGenAISystem::OnTexturePlanReady);
}

FString UGenAISystem::ConstructMasterPrompt(FString UserPrompt, UAssetIndexer* AssetIndexer)
{
    // === SAFETY CHECKS ===
    if (!AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("GenAISystem: AssetIndexer is null"));
        return TEXT("");
    }

    // Only check Mesh and Texture drafts (Lighting draft is removed)
    if (DraftMeshJson.IsEmpty() || DraftTexJson.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("GenAISystem: Missing Draft Plans! Director Prompt may fail."));
    }

    // === STEP 1: HANDLE EMPTY DRAFTS (Safety Fallback) ===
    if (DraftMeshJson.IsEmpty()) DraftMeshJson = TEXT("{\"SpawnRequest\": [], \"ParticleSpawn\": []}");
    if (DraftTexJson.IsEmpty()) DraftTexJson = TEXT("{\"Props\": []}");

    // === STEP 2: PREPARE VERIFIED LISTS ===
    // We use the PRUNED lists from our experts for Meshes and Materials.
    FString MeshListString = PrunedMeshList.IsEmpty() ? TEXT("[]") : FString::Printf(TEXT("[%s]"), *PrunedMeshList);
    FString MaterialString = PrunedTextureList.IsEmpty() ? TEXT("[]") : FString::Printf(TEXT("[%s]"), *PrunedTextureList);

    // We use FULL lists for Tags and PostProcessMaterials
    TArray<FString> ActorTags = AssetIndexer->GetDiscoveredActorTags();
    FString TagString = FString::Join(ActorTags, TEXT("\", \""));

    TArray<FString> AvailablePPMs = AssetIndexer->GetDiscoveredPostProcessNames();
    FString PPMString = FString::Join(AvailablePPMs, TEXT("\", \""));

    // === STEP 3: CONSTRUCT THE DIRECTOR PROMPT ===
    // This prompt instructs the LLM to act as both a Merger (for objects) and a Creator (for lighting).
    FString Payload = FString::Printf(TEXT(
        "You are the Lead Scene Director specializing in Unreal Engine.\n"
        "Two specialized agents have proposed partial plans (Meshes and Textures). Your job is to MERGE them and GENERATE the Lighting/Environment settings.\n\n"
        
        "USER REQUEST: \"%s\"\n\n"

        "=== INPUT 1: MESH DRAFT (Spawns & Particles) ===\n"
        "%s\n\n"
        
        "=== INPUT 2: TEXTURE DRAFT (Materials & Props) ===\n"
        "%s\n\n"
        
        "=== VERIFIED RESOURCE LIST (TRUST THESE) ===\n"
        "The agents have already validated these assets against the game database. THEY EXIST.\n"
        "Valid Meshes: %s\n"
        "Valid Materials: %s\n"
        "Valid PostProcess: [\"%s\"]\n"
        "Valid Tags: [\"%s\"]\n\n"

        "=== INSTRUCTIONS ===\n"
        "1. MERGE: Combine 'SpawnRequest' and 'ParticleSpawn' from Input 1, and 'Props' from Input 2.\n"
        "2. TRUST: The assets in the Verified List are real. Do NOT delete them unless they create a semantic conflict.\n"
        "3. GENERATE LIGHTING (CRITICAL):\n"
        "   - You must create the 'Environment' and 'Lighting' objects from scratch based on the USER REQUEST.\n"
        "   - Analyze the mood (e.g. 'Cyberpunk' = Low Sun Intensity, High Fog Density, Neon Colors; 'Sunny' = High Sun Intensity, Blue Sky).\n"
        "   - Select a valid 'PostProcessingName' from the list above if it fits the theme, otherwise use \"\".\n"
        "4. DENSITY: If Input 1 has multiple items, include them all. Do not summarize.\n"
        "5. OUTPUT: Return strictly valid JSON using the FULL schema below.\n"
        
        "=== FULL JSON SCHEMA ===\n"
        "{\n"
        "  \"ThemeName\": \"descriptive_name\",\n"
        "  \"bModifyEnvironment\": true,\n"
        "  \"bModifyProps\": true,\n"
        "  \"bSpawnActors\": true,\n"
        "  \"TargetPropTags\": [ \"tag1\", \"tag2\" ],\n"
        "  \"Environment\": {\n"
        "    \"FogDensity\": 0.0-5.0,\n"
        "    \"FogColor\": [R,G,B],\n"
        "    \"PostProcessingName\": \"material_name_or_empty\",\n"
        "    \"Lighting\": {\n"
        "      \"SunColor\": [R,G,B],\n"
        "      \"SunIntensity\": 0.0-50.0,\n"
        "      \"SunPitch\": -90.0 to 90.0,\n"
        "      \"SunYaw\": 0.0 to 360.0,\n"
        "      \"SkyLightColor\": [R,G,B],\n"
        "      \"SkyLightIntensity\": 0.0-10.0,\n"
        "      \"SunTemperature\": 1000-15000\n"
        "    }\n"
        "  },\n"
        "  \"Props\": [\n"
        "    {\n"
        "      \"TagName\": \"exact_tag\",\n"
        "      \"PropColor\": [R,G,B],\n"
        "      \"Texture\": {\n"
        "        \"BaseColorPath\": \"material_base_name\",\n"
        "        \"NormalPath\": \"\",\n"
        "        \"RoughnessPath\": \"\",\n"
        "        \"MetallicPath\": \"\",\n"
        "        \"AOPath\": \"\"\n"
        "      },\n"
        "      \"ParticleEffects\": \"\"\n"
        "    }\n"
        "  ],\n"
        "  \"SpawnRequest\": [\n"
        "    {\n"
        "      \"AssetPath\": \"exact_mesh_name\",\n"
        "      \"ObjectName\": \"unique_instance_name\",\n"
        "      \"LocationName\": \"SEMANTIC_LOCATION\",\n"
        "      \"LocationOffset\": [0,0,0],\n"
        "      \"Rotation\": [Pitch,Yaw,Roll],\n"
        "      \"Scale\": [X,Y,Z],\n"
        "      \"Tag\": \"optional_tag\",\n"
        "      \"ClearanceRadius\": 150\n"
        "    }\n"
        "  ],\n"
        "  \"ParticleSpawn\": [\n"
        "    {\n"
        "      \"AssetPath\": \"exact_particle_name\",\n"
        "      \"ObjectName\": \"unique_instance_name\",\n"
        "      \"LocationName\": \"SEMANTIC_LOCATION\",\n"
        "      \"LocationOffset\": [0,0,0],\n"
        "      \"Rotation\": [0,0,0],\n"
        "      \"Scale\": [1,1,1],\n"
        "      \"Tag\": \"optional_tag\",\n"
        "      \"ClearanceRadius\": 150\n"
        "    }\n"
        "  ]\n"
        "}\n\n"
        "Generate the Merged JSON now:"
    ),
    *UserPrompt,
    *DraftMeshJson,     // Input 1 (Meshes)
    *DraftTexJson,      // Input 2 (Textures)
    *MeshListString,    // Verified Meshes
    *MaterialString,    // Verified Materials
    *PPMString,         // Verified PostProcess
    *TagString          // Verified Tags
    );

    return Payload;
}
void UGenAISystem::ExecuteFallbackPlan()
{
	// If the Director fails, we manually stitch the 3 drafts together.
	// This guarantees we get the "Raw" output from Llama 3.3.
    
	UE_LOG(LogTemp, Display, TEXT("🔄 GenAI: Stitching Backup Plan from Drafts..."));

	FEnhancedScenePlan BackupPlan;
    
	// 1. Recover Mesh Data
	if (!DraftMeshJson.IsEmpty()) 
	{
		FEnhancedScenePlan MeshP = UJsonParser::CreatePlan(DraftMeshJson);
		BackupPlan.SpawnRequest = MeshP.SpawnRequest;
		BackupPlan.ParticleSpawns = MeshP.ParticleSpawns;
		UE_LOG(LogTemp, Display, TEXT("   + Recovered %d Spawns from Mesh Draft"), BackupPlan.SpawnRequest.Num());
	}



	// 2. Recover Texture Data
	if (!DraftTexJson.IsEmpty())
	{
		FEnhancedScenePlan TexP = UJsonParser::CreatePlan(DraftTexJson);
		BackupPlan.Props = TexP.Props;
		UE_LOG(LogTemp, Display, TEXT("   + Recovered %d Prop Edits"), BackupPlan.Props.Num());
	}

	BackupPlan.ThemeName = "Fallback: " + LastUserPrompt;
	BackupPlan.bSpawnActors = (BackupPlan.SpawnRequest.Num() > 0);
    
	// Broadcast the Frankenstein plan
	OnThemeDataReady.Broadcast(BackupPlan, LastUserPrompt);
}

