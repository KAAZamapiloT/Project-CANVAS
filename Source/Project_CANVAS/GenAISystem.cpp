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



FString UGenAISystem::ConstructMasterPrompt(
		FString UserPrompt,
		class UAssetIndexer* AssetIndexer
	)
{
    
    TArray<FString> AvailableTags = AssetIndexer->GetDiscoveredActorTags();
    TArray<FString> AvailablePPM=AssetIndexer->GetDiscoveredPostProcessNames();
	
    FString MaterialString = PrunedTextureList;
	FString MeshListString = PrunedMeshList;
    FString TagString = FString::Join(AvailableTags, TEXT("\", \""));
	FString PPMString=FString::Join(AvailablePPM, TEXT("\", \""));
    FString Payload = FString::Printf(TEXT(
        "ROLE: You are the Senior Level Architect for a high-end Unreal Engine 5 project.\n"
        "OBJECTIVE: Review, validate, and integrate proposals from two junior specialists into a cohesive final scene plan.\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "CLIENT ORIGINAL REQUEST\n"
        "═══════════════════════════════════════════════════════════════\n"
        "\"%s\"\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "PROPOSAL 1: STRUCTURAL ENGINEER (Mesh Placement)\n"
        "═══════════════════════════════════════════════════════════════\n"
        "Your structural engineer has calculated spawn locations and submitted:\n"
        "%s\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "PROPOSAL 2: INTERIOR DESIGNER (Surface Materials)\n"
        "═══════════════════════════════════════════════════════════════\n"
        "Your interior designer has selected materials and submitted:\n"
        "%s\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "VERIFIED ASSET CATALOG (Master Inventory)\n"
        "═══════════════════════════════════════════════════════════════\n"
        "You may ONLY use assets from this verified inventory.\n"
        "Do NOT approve any assets not listed here.\n\n"
        
        "Valid Static Meshes:\n"
        "[\"%s\"]\n\n"
        
        "Valid Materials:\n"
        "[\"%s\"]\n\n"
        
        "Valid Post-Process Volumes:\n"
        "[\"%s\"]\n\n"
        
        "Valid Actor Tags:\n"
        "[\"%s\"]\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "YOUR ARCHITECTURAL RESPONSIBILITIES\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        
        "1. VALIDATE STRUCTURAL PROPOSAL (Critical)\n"
        "   • Review all 'SpawnRequest' items from Proposal 1\n"
        "   • Verify every AssetPath exists in Valid Static Meshes list\n"
        "   • Check for spatial conflicts (overlapping ClearanceRadius)\n"
        "   • Approve valid items, flag any hallucinated assets\n"
        "   • Include ALL validated spawns in final output\n\n"
        
        "   • Review all 'ParticleSpawns' items from Proposal 1\n"
        "   • Verify particle names exist in inventory\n"
        "   • Ensure appropriate LocationName usage\n"
        "   • Include ALL validated particle spawns\n\n"
        
        "2. VALIDATE SURFACE PROPOSAL (Critical)\n"
        "   • Review all 'Props' items from Proposal 2\n"
        "   • Verify every TagName exists in Valid Actor Tags\n"
        "   • Verify every material name exists in Valid Materials\n"
        "   • Check PropColor values are valid (0-255 range)\n"
        "   • Include ALL validated props in final output\n\n"
        
        "3. DESIGN ATMOSPHERIC LIGHTING (Your Unique Contribution)\n"
        "   The junior specialists left lighting and environment to you.\n"
        "   Create dramatic lighting that matches the client's theme:\n\n"
        
        "   THEME-BASED LIGHTING PRESETS:\n\n"
        
        "   CYBERPUNK / SCI-FI:\n"
        "     • SunColor: [0.3, 0.5, 0.8] (cool blue)\n"
        "     • SunIntensity: 5.0 (dim, artificial feel)\n"
        "     • SunPitch: -30.0 (low angle)\n"
        "     • SunYaw: 180.0\n"
        "     • SkyLightColor: [0.2, 0.3, 0.5] (dark blue ambient)\n"
        "     • SkyLightIntensity: 0.5 (moody)\n"
        "     • FogDensity: 0.8 (heavy atmosphere)\n"
        "     • FogColor: [60, 80, 120] (blue-grey)\n\n"
        
        "   NATURE / FOREST:\n"
        "     • SunColor: [1.0, 0.95, 0.8] (warm daylight)\n"
        "     • SunIntensity: 15.0 (bright, natural)\n"
        "     • SunPitch: -60.0 (overhead sun)\n"
        "     • SunYaw: 90.0\n"
        "     • SkyLightColor: [0.4, 0.6, 0.8] (sky blue)\n"
        "     • SkyLightIntensity: 2.0 (bright ambient)\n"
        "     • FogDensity: 0.2 (light morning mist)\n"
        "     • FogColor: [200, 220, 240] (light blue)\n\n"
        
        "   HORROR / ABANDONED:\n"
        "     • SunColor: [0.5, 0.5, 0.6] (desaturated)\n"
        "     • SunIntensity: 3.0 (very dim)\n"
        "     • SunPitch: -15.0 (low, ominous)\n"
        "     • SunYaw: 270.0\n"
        "     • SkyLightColor: [0.1, 0.1, 0.15] (almost black)\n"
        "     • SkyLightIntensity: 0.3 (minimal)\n"
        "     • FogDensity: 1.5 (thick, oppressive)\n"
        "     • FogColor: [80, 80, 90] (dark grey)\n\n"
        
        "   DESERT / SUNSET:\n"
        "     • SunColor: [1.0, 0.7, 0.4] (warm orange)\n"
        "     • SunIntensity: 20.0 (intense)\n"
        "     • SunPitch: -20.0 (sunset angle)\n"
        "     • SunYaw: 0.0\n"
        "     • SkyLightColor: [0.8, 0.5, 0.3] (orange ambient)\n"
        "     • SkyLightIntensity: 1.5\n"
        "     • FogDensity: 0.3 (heat haze)\n"
        "     • FogColor: [255, 200, 150] (warm haze)\n\n"
        
        "   WINTER / ICE:\n"
        "     • SunColor: [0.8, 0.9, 1.0] (cool white)\n"
        "     • SunIntensity: 12.0 (bright but cold)\n"
        "     • SunPitch: -45.0\n"
        "     • SunYaw: 135.0\n"
        "     • SkyLightColor: [0.7, 0.8, 0.9] (pale blue)\n"
        "     • SkyLightIntensity: 1.8\n"
        "     • FogDensity: 0.5 (snow/ice particles)\n"
        "     • FogColor: [220, 230, 255] (pale blue-white)\n\n"
        
        "   FIELD EXPLANATIONS:\n"
        "   • SunColor: RGB floats 0.0-1.0 (NOT 0-255)\n"
        "   • SunIntensity: 0.0-50.0 (typical range 5-20)\n"
        "   • SunPitch: -90 (straight down) to 0 (horizon) to 90 (straight up)\n"
        "   • SunYaw: 0 (north) to 360 degrees\n"
        "   • SkyLightColor: RGB floats 0.0-1.0\n"
        "   • SkyLightIntensity: 0.0-10.0 (typical range 0.5-3.0)\n"
        "   • SunTemperature: 1000-15000 Kelvin (optional, use if bUseTemperature=true)\n"
        "   • FogDensity: 0.0-5.0 (0=clear, 2.0+=very thick)\n"
        "   • FogColor: RGB integers 0-255\n"
        "   • PostProcessingName: Select from Valid Post-Process list or leave empty\n\n"
        
        "4. SET MODIFICATION FLAGS\n"
        "   • bModifyEnvironment: true (you're defining lighting/fog)\n"
        "   • bModifyProps: true (if Proposal 2 has props)\n"
        "   • bSpawnActors: true (if Proposal 1 has spawns)\n"
        "   • TargetPropTags: Leave empty [] (modify all props)\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "COMPLETE JSON SCHEMA\n"
        "═══════════════════════════════════════════════════════════════\n"
        
        "{\n"
        "  \"ThemeName\": \"descriptive_name\",\n"
        "  \"bModifyEnvironment\": true,\n"
        "  \"bModifyProps\": true,\n"
        "  \"bSpawnActors\": true,\n"
        "  \"TargetPropTags\": [],\n"
        "  \"Environment\": {\n"
        "    \"FogDensity\": 0.0-5.0,\n"
        "    \"FogColor\": [R, G, B],\n"
        "    \"PostProcessingName\": \"name_from_list_or_empty\",\n"
        "    \"Lighting\": {\n"
        "      \"SunColor\": [R, G, B],\n"
        "      \"SunIntensity\": 0.0-50.0,\n"
        "      \"SunPitch\": -90.0 to 90.0,\n"
        "      \"SunYaw\": 0.0-360.0,\n"
        "      \"SkyLightColor\": [R, G, B],\n"
        "      \"SkyLightIntensity\": 0.0-10.0,\n"
        "      \"SunTemperature\": 1000.0-15000.0,\n"
        "      \"bUseTemperature\": false\n"
        "    }\n"
        "  },\n"
        "  \"Props\": [\n"
        "    {\n"
        "      \"TagName\": \"exact_tag_from_validation\",\n"
        "      \"PropColor\": [R, G, B],\n"
        "      \"Texture\": {\n"
        "        \"BaseColorPath\": \"material_name\",\n"
        "        \"NormalPath\": \"optional_name_or_empty\",\n"
        "        \"RoughnessPath\": \"optional_name_or_empty\",\n"
        "        \"MetallicPath\": \"optional_name_or_empty\",\n"
        "        \"AOPath\": \"optional_name_or_empty\"\n"
        "      },\n"
        "      \"ParticleEffects\": \"\"\n"
        "    }\n"
        "  ],\n"
        "  \"SpawnRequest\": [\n"
        "    {\n"
        "      \"AssetPath\": \"exact_mesh_name\",\n"
        "      \"ObjectName\": \"unique_instance_name\",\n"
        "      \"LocationName\": \"SEMANTIC_LOCATION\",\n"
        "      \"LocationOffset\": [X, Y, Z],\n"
        "      \"Rotation\": [Pitch, Yaw, Roll],\n"
        "      \"Scale\": [X, Y, Z],\n"
        "      \"Tag\": \"optional_tag\",\n"
        "      \"ClearanceRadius\": 150.0\n"
        "    }\n"
        "  ],\n"
        "  \"ParticleSpawns\": [\n"
        "    {\n"
        "      \"AssetPath\": \"exact_particle_name\",\n"
        "      \"ObjectName\": \"unique_instance_name\",\n"
        "      \"LocationName\": \"SEMANTIC_LOCATION\",\n"
        "      \"LocationOffset\": [X, Y, Z],\n"
        "      \"Rotation\": [Pitch, Yaw, Roll],\n"
        "      \"Scale\": [X, Y, Z],\n"
        "      \"Tag\": \"optional_tag\",\n"
        "      \"ClearanceRadius\": 50.0\n"
        "    }\n"
        "  ]\n"
        "}\n\n"
        "═══════════════════════════════════════════════════════════════\n"
        "VALIDATION CHECKLIST (Critical - Must Verify)\n"
        "═══════════════════════════════════════════════════════════════\n"
        
        "STRUCTURAL VALIDATION:\n"
        "✓ Every SpawnRequest AssetPath exists in Valid Static Meshes\n"
        "✓ Every ParticleSpawns AssetPath exists in particle inventory\n"
        "✓ No duplicate ObjectName values\n"
        "✓ All numeric values within valid ranges\n"
        "✓ Array name is 'ParticleSpawns' (with 's')\n\n"
        
        "SURFACE VALIDATION:\n"
        "✓ Every Props TagName exists in Valid Actor Tags\n"
        "✓ Every BaseColorPath exists in Valid Materials\n"
        "✓ PropColor values are integers 0-255\n"
        "✓ Optional texture paths are valid names or empty strings\n\n"
        
        "LIGHTING VALIDATION:\n"
        "✓ SunColor and SkyLightColor use floats 0.0-1.0\n"
        "✓ FogColor uses integers 0-255\n"
        "✓ Intensity values within 0-50 range\n"
        "✓ Pitch/Yaw/Roll within specified ranges\n"
        "✓ PostProcessingName from valid list or empty string\n\n"
        
        "STRUCTURAL VALIDATION:\n"
        "✓ All required fields present\n"
        "✓ All boolean flags set appropriately\n"
        "✓ Arrays properly formatted with brackets\n"
        "✓ No trailing commas\n"
        "✓ Valid JSON syntax\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "CONFLICT RESOLUTION GUIDELINES\n"
        "═══════════════════════════════════════════════════════════════\n"
        
        "IF ASSET HALLUCINATION DETECTED:\n"
        "• Remove the invalid spawn from SpawnRequest/ParticleSpawns\n"
        "• Do NOT include assets not in verified catalog\n"
        "• Continue with remaining valid spawns\n\n"
        
        "IF TAG MISMATCH DETECTED:\n"
        "• Remove Props entries with invalid TagName\n"
        "• Keep only Props with tags from Valid Actor Tags\n\n"
        
        "IF MATERIAL HALLUCINATION DETECTED:\n"
        "• Replace invalid material with closest valid alternative\n"
        "• Or remove the Props entry if no suitable match\n\n"
        
        "IF SPATIAL CONFLICTS DETECTED:\n"
        "• Adjust LocationOffset to separate overlapping objects\n"
        "• Increase ClearanceRadius for large objects\n"
        "• Move conflicting items to different LocationName zones\n\n"
        
        "IF POST-PROCESS NAME INVALID:\n"
        "• Set PostProcessingName to empty string \"\"\n"
        "• Let default post-processing handle the scene\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "EDGE CASE HANDLING\n"
        "═══════════════════════════════════════════════════════════════\n"
        
        "• If both proposals are empty: Create minimal lighting-only scene\n"
        "• If only meshes provided: Add default neutral lighting\n"
        "• If only textures provided: Add atmospheric lighting matching theme\n"
        "• If client request conflicts with proposals: Prioritize client vision\n"
        "• If theme unclear: Use balanced neutral lighting preset\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "OUTPUT INSTRUCTIONS\n"
        "═══════════════════════════════════════════════════════════════\n"
        
        "1. Return ONLY valid JSON (no markdown blocks, no explanations)\n"
        "2. Start with '{' and end with '}'\n"
        "3. Do not include ```json code fences\n"
        "4. Ensure all arrays, objects properly closed\n"
        "5. No trailing commas after last array/object elements\n"
        "6. All strings properly quoted\n"
        "7. All numeric values unquoted\n"
        "8. Boolean values as true/false (not \"true\"/\"false\")\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "FINAL REMINDER\n"
        "═══════════════════════════════════════════════════════════════\n"
        
        "You are the final authority. Your output will be parsed directly by C++.\n"
        "Invalid JSON will crash the system. Invalid asset names will cause runtime errors.\n"
        "Validate everything. Include only verified, approved assets.\n\n"
        
        "GENERATE INTEGRATED MASTER SCENE PLAN NOW:"
    ),
    *UserPrompt,
    *DraftMeshJson,
    *DraftTexJson,
    *MeshListString,
    *MaterialString,
    *PPMString,
    *TagString
    );

    return Payload;
}


// ============================================================================
// PROMPT 3: ARCHITECT (Master Integration Agent)
// ============================================================================


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

