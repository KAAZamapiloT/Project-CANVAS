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
#include "IntentionResolverLLM.h"
#include"PaintingResolverLLM.h"
#include "MeshResolverLLM.h"
#include "TextureResolverLLM.h"
#include "ToolContextInterfaces.h"
#include "Components/Image.h"

// Top of GenAISystem.cpp
TWeakObjectPtr<UGenAISystem> UGenAISystem::GlobalActiveAuthority = nullptr;
bool UGenAISystem::bIsGlobalPipelineBusy = false;
int32 UGenAISystem::GlobalSequenceNonce = 0;

void UGenAISystem::RequestSceneChange(FString UserPrompt,UWorld* WorldContext,USceneHistoryManager* HistoryManager)
{
	// 1. Check Global Busy Flag (Shared by everyone)
	if (bIsGlobalPipelineBusy) {
		UE_LOG(LogTemp, Warning, TEXT("🛑 GenAI BLOCKED: Another build is already in progress."));
		return;
	}

	// 2. Claim Authority
	// The first instance to hit this line becomes the 'Alpha'
	GlobalActiveAuthority = this;
	bIsGlobalPipelineBusy = true;
    
	// 3. Update Sequence (The Nonce)
	GlobalSequenceNonce++;
	MyActiveRequestID = GlobalSequenceNonce;

	UE_LOG(LogTemp, Warning, TEXT("🚀 GenAI START: Instance [%p] taking Authority. ID: %d"), this, MyActiveRequestID);

	if (!WorldContext)
	{
		ResetPipeline();
		return;
	}
	UWorld* SafeWorld = WorldContext;
	if (!SafeWorld)
	{
		SafeWorld = GetWorld();
	}

	// 2. Validate Critical Systems
	if (!SafeWorld)
	{
		UE_LOG(LogTemp, Error, TEXT("GenAISystem: Cannot resolve a valid World Context!"));
		ResetPipeline();
		return;
	}
	// 1. Get the GameInstance and AssetIndexer
	UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(WorldContext);
	USceneStateTracker* Tracker = GameInstance->GetSubsystem<USceneStateTracker>();
    if (!GameInstance || !Tracker->AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("GenAISystem: Cannot find GameInstance or AssetIndexer!"));
    	ResetPipeline();
        return;
    }

    // 2. Check if the asset list is ready
    if (!Tracker->AssetIndexer->IsScanComplete())
    {
        UE_LOG(LogTemp, Warning, TEXT("GenAISystem: Asset scan is not complete. Please wait."));
    	ResetPipeline();
        return;
    }

    
	LastUserPrompt = UserPrompt;
	CachedWorld = SafeWorld;
	CachedHistory = HistoryManager;

// RESET
	bHasSynthesized = false; 
	bIsMeshReady = false;
	bIsTexReady = false;
	bIsPaintingReady = false;
	bIsIntentReady = false;
	
	DraftPaintingJson = "";
	DraftMeshJson = "";
	DraftTexJson = "";
	PrunedMeshList = "";
	PrunedTextureList = "";

	TArray<FString> AllConcepts;
	AllConcepts.Append(Tracker->AssetIndexer->GetSmartMeshList());
	AllConcepts.Append(Tracker->AssetIndexer->GetMaterialBaseNames());

	// 3. START STAGE 1: INTENTION
	UE_LOG(LogTemp, Display, TEXT("🤖 Phase 1: Analyzing Intention..."));
	if (bIntent)

	{
		FDatabaseVocabulary Vocabulary=Tracker->AssetIndexer->GetCategorizedVocabulary();
		TArray<FString> AllMeshes=Vocabulary.MeshTags;
		TArray<FString> AllTexture=Vocabulary.TextureTags;
		TArray<FString> AllParticles=Vocabulary.ParticleTags;
		IntentionL->RequestIntention(UserPrompt, AllMeshes, AllTexture, AllParticles);
	}else
	{
		UE_LOG(LogTemp, Display, TEXT("🤖 Legacy Mode: Skipping Intention, calling all agents directly..."));
        
		// Pass the resolved SafeWorld to all agents
		MeshL->RequestPlan(UserPrompt, SafeWorld, HistoryManager);
		TexL->RequestPlan(UserPrompt, SafeWorld, HistoryManager);
        
		if (bPaintL)
		{
			PaintL->RequestPaintingPlan(UserPrompt, SafeWorld, Tracker->AssetIndexer);
		}
		else
		{
			// If Painting Resolver is disabled, mark ready immediately to unblock synthesis
			bIsPaintingReady = true; 
		}
		
	}
	
	
	
	
}

void UGenAISystem::OnLLMResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{

	
	if (!bWasSuccessful || !Response.IsValid())
    {
		ResetPipeline();
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
		// 🛡️ REFINED VALIDATION LOGIC
		// =========================================================

		// A plan is valid if it has manual spawns OR procedural layout commands.
		bool bMasterHasContent = (MasterPlan.SpawnRequest.Num() > 0 || MasterPlan.LayoutCommands.Num() > 0);

		// We only consider it a failure if the Master returned nothing but our agents had ideas.
		bool bMasterFailed = (!bMasterHasContent && (!DraftMeshJson.IsEmpty() || !DraftPaintingJson.IsEmpty()));

		if (bMasterFailed)
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ GenAI: Master returned no content! Reverting to Draft Plans..."));
			ExecuteFallbackPlan();
		}
		else
		{
			// Auto-enable spawning if we have layout commands
			if (MasterPlan.LayoutCommands.Num() > 0) MasterPlan.bSpawnActors = true;

			UE_LOG(LogTemp, Warning, TEXT("✅ GenAI: Master Success! Spawning %d items, %d Layout Cmds."), 
				MasterPlan.SpawnRequest.Num(), MasterPlan.LayoutCommands.Num());
        
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
	// [FIX 1] The Gatekeeper: If we already built this request, STOP.
	if (bHasSynthesized) 
	{
		UE_LOG(LogTemp, Warning, TEXT("🛑 GenAISystem: Synthesis triggered, but plan already built. Ignoring."));
		return;
	}

	// 2. Check Flags (Wait for everyone)
	// Note: If you want Speed Mode to be "First Come First Served", keep your current logic.
	// But for a stable scene, you MUST wait.
	if (!bIsMeshReady)
	{
		UE_LOG(LogTemp, Warning, TEXT("⏳ Waiting for Mesh Agent..."));
		return; 
	}

	if (!bIsTexReady)
	{
		UE_LOG(LogTemp, Warning, TEXT("⏳ Waiting for Texture Agent..."));
		return; 
	}

	// 3. Optional: Painting Check
	if (bPaintL && !bIsPaintingReady)
	{
		UE_LOG(LogTemp, Warning, TEXT("⏳ Waiting for Painting Agent..."));
		return;
	}

	// ==========================================
	// 🚀 EXECUTION START
	// ==========================================
    
	// [FIX 2] Close the Gate immediately
	bHasSynthesized = true; 

	UE_LOG(LogTemp, Log, TEXT("🚀 ALL SYSTEMS READY. Triggering Single Master Build..."));
	
	
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
	FString MasterPrompt;
	// 4. Proceed
	if (!bSpeedMode){
		MasterPrompt = ConstructMasterPrompt(UserPrompt, Tracker->AssetIndexer);
	}else
	{
		MasterPrompt=ConstructPrunedMasterPrompt(UserPrompt,Tracker->AssetIndexer);
	}
	
	
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
	if (!IsAuthorized()) return; // ✅ CRITICAL LATCH
	if (bIsMeshReady) return;
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
	if (!IsAuthorized()) return; // ✅ CRITICAL LATCH
	if (bIsTexReady) return;
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

void UGenAISystem::OnPaintingPlanReady(FString Plan, FString Summary)
{
	if (!IsAuthorized()) return; // ✅ CRITICAL LATCH
	if (bIsPaintingReady){
		return;
	}
	UE_LOG(LogTemp, Display, TEXT("✅ Painting Agent Finished. Summary: %s"), *Summary);

	DraftPaintingJson = Plan;
	bIsPaintingReady = true;

	// Trigger Synthesis Check
	// We pass the cached context vars you saved in RequestSceneChange
	if (CachedWorld.IsValid())
	{
		AttemptSynthesis(LastUserPrompt, CachedWorld.Get(), CachedHistory);
	}
	else
	{
		AttemptSynthesis(LastUserPrompt, GetWorld(), CachedHistory);
	}
}

void UGenAISystem::Initialize()
{
	UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld());
	if (!GI) return;
	MeshL=UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<UMeshResolverLLM>();
	
	TexL= UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<UTextureResolverLLM>();

	IntentionL = UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<UIntentionResolverLLM>();

	PaintL=GI->GetSubsystem<UPaintingResolverLLM>();
	
	MeshL->OnMeshPlanReady.AddDynamic(this,&UGenAISystem::OnMeshPlanReady);

	TexL->OnTexturePlanReady.AddDynamic(this,&UGenAISystem::OnTexturePlanReady);

	IntentionL->OnIntentionReady.AddDynamic(this,&UGenAISystem::OnIntentionReady);

	PaintL->OnPaintingPlanReady.AddDynamic(this,&UGenAISystem::OnPaintingPlanReady);

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

	IntentionL->OnIntentionReady.RemoveDynamic(this,&UGenAISystem::OnIntentionReady);

	PaintL->OnPaintingPlanReady.RemoveDynamic(this,&UGenAISystem::OnPaintingPlanReady);
}

void UGenAISystem::OnIntentionReady(const TArray<FString>&  RelevantMeshes,const TArray<FString>&  RelevantTextures,
		const TArray<FString>&  RelevantParticles)
{
	// [THE LATCH] Only the Alpha instance with the correct ID proceeds.
	if (!IsAuthorized()) return;
	
	// [ADD THIS LATCH]
	if (bIsIntentReady)
	{
		UE_LOG(LogTemp, Warning, TEXT("🛑 Duplicate Intention received. Dropping."));
		return;
	}
	bIsIntentReady = true;
	
    //UE_LOG(LogTemp, Display, TEXT("✅ Phase 1: Intention identified %d categories."), Keywords.Num());
	// =========================================================
	// 1. ROBUST WORLD RECOVERY (Prevents "Invalid World" crashes)
	// =========================================================
	UWorld* TargetWorld = CachedWorld.Get();
    
	if (!TargetWorld)
	{
		// Fallback: Try to get world from the GameInstance (we are a Subsystem)
		if (UGameInstance* GI = Cast<UGameInstance>(GetOuter()))
		{
			TargetWorld = GI->GetWorld();
		}
		// Fallback: Try standard global lookup
		if (!TargetWorld)
		{
			TargetWorld = UGameplayStatics::GetGameInstance(this)->GetWorld();
		}
	}

	if (!TargetWorld)
	{
		bIsGlobalPipelineBusy = false; // ✅ UNLOCK ON FAILURE
		UE_LOG(LogTemp, Error, TEXT("❌ GenAISystem: CRITICAL - Lost World Context. Aborting Pipeline."));
		return;
	}
    USceneStateTracker* Tracker = UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<USceneStateTracker>();
    if (!Tracker || !Tracker->AssetIndexer)
    {
    	bIsGlobalPipelineBusy = false; // ✅ UNLOCK ON FAILURE
        UE_LOG(LogTemp, Error, TEXT("GenAISystem: Tracker/Indexer invalid in OnIntentionReady"));
        return;
    }

    // 1. SMART EXPANSION (The New Robust Logic)
    // This splits the keywords into valid Meshes, Particles, and Textures
	FSmartAssetSelection PrunedContext = Tracker->AssetIndexer->ExpandKeywordsToCollection(RelevantMeshes
		, RelevantTextures, RelevantParticles,100);
    // 2. PREPARE PAYLOADS 
    PrunedMeshesContext=PrunedContext.Meshes;
	PrunedTexturesContext=PrunedContext.Textures;
	PrunedParticlesContext=PrunedContext.Particles;
    
    // 4. DISPATCH TO RESOLVERS (With Specific Lists)
    UE_LOG(LogTemp, Display, TEXT("🤖 Phase 2: Dispatching Specialists..."));
    UE_LOG(LogTemp, Display, TEXT("   • Mesh Context: %d items"), RelevantMeshes.Num());
    UE_LOG(LogTemp, Display, TEXT("   • Texture Context: %d items"), RelevantTextures.Num());
	// Inject Data Directly
	PrunedMeshList = FString::Join( PrunedMeshesContext, TEXT(","));
	PrunedTextureList = FString::Join(PrunedTexturesContext, TEXT(","));
	
	if (bSpeedMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚡ SPEED MODE: Bypassing Specialists."));

		
        
		// Clear drafts to force Master to generate fresh JSON
		DraftMeshJson = ""; 
		DraftTexJson = "";
		DraftPaintingJson = "";

		// Open ALL Gates
		bIsMeshReady = true;
		bIsTexReady = true;
		bIsPaintingReady = true;

		// Trigger Master Immediately
		AttemptSynthesis(LastUserPrompt, TargetWorld, CachedHistory);
	}
	else
	{
		// --- AGENT 1: MESH RESOLVER ---
		if (RelevantMeshes.Num() > 0)
		{
			MeshL->RequestPlan_Pruned(LastUserPrompt, PrunedMeshList, TargetWorld, CachedHistory);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ No meshes found. Auto-completing Mesh Agent."));
			OnMeshPlanReady(TEXT("{\"SpawnRequest\": [], \"ParticleSpawns\": []}"), TEXT(""));
		}

		// --- AGENT 2: TEXTURE RESOLVER ---
		if (RelevantTextures.Num() > 0)
		{
			TexL->RequestPlan_Pruned(LastUserPrompt, PrunedTextureList, TargetWorld, CachedHistory);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("⚠️ No textures found. Auto-completing Texture Agent."));
			OnTexturePlanReady(TEXT("{\"Props\": []}"), TEXT(""));
		}

		// --- AGENT 3: PAINTING RESOLVER (THE FIX) ---
		// We MUST trigger this or AttemptSynthesis will wait forever.
		if (bPaintL && PaintL)
		{
			PaintL->RequestPaintingPlan(LastUserPrompt, TargetWorld, Tracker->AssetIndexer);
		}
		else
		{
			// If disabled/missing, mark ready immediately to unblock pipeline
			bIsPaintingReady = true; 
		}
	}
    
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
        "OBJECTIVE: Review, validate, and integrate proposals from two/three junior specialists into a cohesive final scene plan.\n\n"
        "You are a Level Designer. You must select items from the 'Valid Static Meshes' list.\n"
        "Put the name of the item into the 'AssetPath' field. I will resolve the path for you.\n"
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
	    "PROPOSAL 3: LEVEL DIRECTOR (Layout Commands)\n"
	    "═══════════════════════════════════════════════════════════════\n"
     	"Your Level Director has issued these procedural commands:\n"
	    "%s\n\n"
	    
        "═══════════════════════════════════════════════════════════════\n"
        "VERIFIED ASSET CATALOG (Master Inventory)\n"
        "═══════════════════════════════════════════════════════════════\n"
        "You may ONLY use assets from this verified inventory.\n"
        "Assets listed by junior specialists are also valid even if they are not contained in your inventory.\n\n"
        
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
 "1. VALIDATE & RESOLVE STRUCTURAL PROPOSAL\n"
 "   • Review 'SpawnRequest' items from Proposal 1 (The Draft)\n"
 "   • The Draft may use generic names (e.g., 'Tree', 'Chair')\n" 
 "   • Search the 'Valid Static Meshes' list for the best matching specific asset\n" 
 "   • REPLACE the generic AssetPath with the specific one from the list\n"
 "   • [NEW RULE] If the Proposal uses a specific asset name that LOOKS valid (e.g., SM_Specific_Prop) but isn't in your list, KEEP IT. Trust the Structural Engineer.\n" // <--- The Fix
 "   • Only reject obvious hallucinations (e.g., 'StarWars_XWing' in a medieval game).\n"
 
        "2. VALIDATE SURFACE PROPOSAL (Critical)\n"
        "   • Review all 'Props' items from Proposal 2\n"
        "   • Verify every TagName exists in Valid Actor Tags\n"
        "   • Verify every material name exists in Valid Materials\n"
        "   • Check PropColor values are valid (0-255 range)\n"
        "   • Include ALL validated props in final output\n\n"
        "3. INTEGRATE LAYOUT COMMANDS (CRITICAL)\n"
   "   • Review 'PaintingCommands' from Proposal 3\n"
   "   • YOU MUST COPY ALL VALID COMMANDS into the final JSON 'LayoutCommands' array\n" // Stronger wording
   "   • DO NOT return an empty array if Proposal 3 has content\n"
   "   • Ensure 'Tool' is valid (FILL_PERIMETER, SCATTER, GRID, RING)\n"
   "   • Ensure 'TargetZone' exists in the map\n"
        "4. DESIGN ATMOSPHERIC LIGHTING (Your Unique Contribution)\n"
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
        
        "5. SET MODIFICATION FLAGS\n"
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
        "  ],\n"
        "  \"LayoutCommands\": [\n"
			   "    {\n"
			   "      \"Tool\": \"FILL_PERIMETER\",               // or SCATTER, GRID, RING\n"
			   "      \"TargetZone\": \"zone_name\",             // semantic zone identifier\n"
			   "      \"Archetype\": \"optional_archetype\",     // optional preset\n"
			   "      \"Style\": {\n"
			   "        \"MeshKeyword\": \"mesh_search_term\",   // partial name to match\n"
			   "        \"MaterialKeyword\": \"mat_search_term\" // partial name to match\n"
			   "      },\n"
			   "      \"Settings\": {                            // tool-specific parameters\n"
			   "        \"Spacing\": 200.0,\n"
			   "        \"Count\": 10,\n"
			   "        \"Radius\": 500.0\n"
			   "      }\n"
			   "    }\n"
			   "  ]\n"
        "}\n\n"
        "═══════════════════════════════════════════════════════════════\n"
        "VALIDATION CHECKLIST (Critical - Must Verify)\n"
        "═══════════════════════════════════════════════════════════════\n"
        
        "STRUCTURAL VALIDATION:\n"
        "✓ Every SpawnRequest AssetPath exists in Valid Static Meshes or Is Given by junior specialist \n"
        "✓ Every ParticleSpawns AssetPath exists in particle inventory or Is Given by junior specialist\n"
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
        "• If only meshes provided: Create lighting according to theme\n"
        "• If only textures provided: Add atmospheric lighting matching theme\n"
        "• If client request conflicts with proposals: Prioritize client vision\n"
        "• If theme unclear: Use what prompt says give anything that closely matches it\n\n"
        
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
        "Invalid JSON will crash the system. Invalid asset names will cause runtime errors.\n\n"
        
        "GENERATE INTEGRATED MASTER SCENE PLAN NOW:"
    ),
    *UserPrompt,
    *DraftMeshJson,
    *DraftTexJson,
    *DraftPaintingJson,
    *MeshListString,
    *MaterialString,
    *PPMString,
    *TagString
    );

    return Payload;
}

FString UGenAISystem::ConstructPrunedMasterPrompt(FString UserPrompt, class UAssetIndexer* AssetIndexer)
{
	TArray<FString> AvailableTags = AssetIndexer->GetDiscoveredActorTags();
    TArray<FString> AvailablePPM = AssetIndexer->GetDiscoveredPostProcessNames();
    
    // Get pruned asset lists
    FString ValidMeshString = PrunedMeshList;     
    FString ValidMatString = PrunedTextureList;
    FString TagString = FString::Join(AvailableTags, TEXT("\", \""));
    FString PPMString = FString::Join(AvailablePPM, TEXT("\", \""));
    
    FString Payload = FString::Printf(TEXT(
        "═══════════════════════════════════════════════════════════════\n"
        "ROLE: UNREAL ENGINE 5 SENIOR LEVEL DESIGNER\n"
        "═══════════════════════════════════════════════════════════════\n"
        "You are an experienced UE5 level designer creating complete scene plans from client requests.\n"
        "Your output will be directly parsed by C++ into an FEnhancedScenePlan structure.\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "CLIENT REQUEST\n"
        "═══════════════════════════════════════════════════════════════\n"
        "\"%s\"\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "AVAILABLE ASSET CATALOG (STRICT INVENTORY)\n"
        "═══════════════════════════════════════════════════════════════\n"
        "⚠️ CRITICAL: You may ONLY use assets from this verified inventory.\n"
        "Assets not listed here do NOT exist and will cause runtime crashes.\n\n"
        
        "Valid Static Meshes:\n"
        "[\"%s\"]\n\n"
        
        "Valid Materials:\n"
        "[\"%s\"]\n\n"
        
        "Valid Post-Process Volumes:\n"
        "[\"%s\"]\n\n"
        
        "Valid Actor Tags (for Props):\n"
        "[\"%s\"]\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "YOUR DESIGN RESPONSIBILITIES\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        
        "1. INTERPRET CLIENT VISION\n"
        "   • Understand the requested theme, mood, and scene type\n"
        "   • Identify key elements needed (structures, props, atmosphere)\n"
        "   • Plan a cohesive visual experience\n\n"
        
        "2. DESIGN ATMOSPHERIC LIGHTING & ENVIRONMENT\n"
        "   • Select lighting that matches the theme mood\n"
        "   • Configure fog for atmospheric depth\n"
        "   • Choose appropriate post-processing (or leave empty)\n"
        "   • Set bModifyEnvironment = true\n\n"
        
        "   THEME-BASED LIGHTING PRESETS:\n\n"
        
        "   CYBERPUNK / SCI-FI:\n"
        "     SunColor: [0.3, 0.5, 0.8] (cool blue)\n"
        "     SunIntensity: 5.0\n"
        "     SunPitch: -30.0\n"
        "     SunYaw: 180.0\n"
        "     SkyLightColor: [0.2, 0.3, 0.5]\n"
        "     SkyLightIntensity: 0.5\n"
        "     FogDensity: 0.8\n"
        "     FogColor: [60, 80, 120]\n\n"
        
        "   NATURE / FOREST:\n"
        "     SunColor: [1.0, 0.95, 0.8] (warm daylight)\n"
        "     SunIntensity: 15.0\n"
        "     SunPitch: -60.0\n"
        "     SunYaw: 90.0\n"
        "     SkyLightColor: [0.4, 0.6, 0.8]\n"
        "     SkyLightIntensity: 2.0\n"
        "     FogDensity: 0.2\n"
        "     FogColor: [200, 220, 240]\n\n"
        
        "   HORROR / ABANDONED:\n"
        "     SunColor: [0.5, 0.5, 0.6] (desaturated)\n"
        "     SunIntensity: 3.0\n"
        "     SunPitch: -15.0\n"
        "     SunYaw: 270.0\n"
        "     SkyLightColor: [0.1, 0.1, 0.15]\n"
        "     SkyLightIntensity: 0.3\n"
        "     FogDensity: 1.5\n"
        "     FogColor: [80, 80, 90]\n\n"
        
        "   DESERT / SUNSET:\n"
        "     SunColor: [1.0, 0.7, 0.4] (warm orange)\n"
        "     SunIntensity: 20.0\n"
        "     SunPitch: -20.0\n"
        "     SunYaw: 0.0\n"
        "     SkyLightColor: [0.8, 0.5, 0.3]\n"
        "     SkyLightIntensity: 1.5\n"
        "     FogDensity: 0.3\n"
        "     FogColor: [255, 200, 150]\n\n"
        
        "   WINTER / ICE:\n"
        "     SunColor: [0.8, 0.9, 1.0] (cool white)\n"
        "     SunIntensity: 12.0\n"
        "     SunPitch: -45.0\n"
        "     SunYaw: 135.0\n"
        "     SkyLightColor: [0.7, 0.8, 0.9]\n"
        "     SkyLightIntensity: 1.8\n"
        "     FogDensity: 0.5\n"
        "     FogColor: [220, 230, 255]\n\n"
        
        "   LIGHTING FIELD REFERENCE:\n"
        "   • SunColor/SkyLightColor: RGB floats 0.0-1.0 (NOT 0-255)\n"
        "   • SunIntensity: 0.0-50.0 (typical: 5-20)\n"
        "   • SunPitch: -90 (overhead) to 0 (horizon) to 90 (up)\n"
        "   • SunYaw: 0-360 degrees (0=north, 90=east, 180=south, 270=west)\n"
        "   • SkyLightIntensity: 0.0-10.0 (typical: 0.5-3.0)\n"
        "   • FogDensity: 0.0-5.0 (0=clear, 2.0+=very thick)\n"
        "   • FogColor: RGB integers 0-255\n"
        "   • SunTemperature: 1000-15000 Kelvin (optional)\n"
        "   • bUseTemperature: false (use SunColor instead)\n\n"
        
        "3. SELECT & TEXTURE EXISTING PROPS (OPTIONAL)\n"
        "   • Review Valid Actor Tags - these are props already in the scene\n"
        "   • Assign materials from Valid Materials list\n"
        "   • Set PropColor for tinting (RGB 0-255)\n"
        "   • Each TagName must exist in Valid Actor Tags\n"
        "   • Each BaseColorPath must exist in Valid Materials\n"
        "   • Set bModifyProps = true if you modify any props\n"
        "   • Leave TargetPropTags empty to modify all props\n\n"
        
        "4. PLAN ACTOR SPAWNS (HIGH-LEVEL LOCATIONS)\n"
        "   ⚠️ LOCATION HANDLING:\n"
        "   • Use SEMANTIC location names (e.g., \"PLAYER_FRONT\", \"Arena_Center\", \"Room_Corner\")\n"
        "   • The location resolver will handle exact coordinates later\n"
        "   • You can specify LocationOffset for relative positioning (e.g., [0, 0, 200] for \"200 units up\")\n"
        "   • SpawnLocation field will be populated by the resolver - you can leave it at [0,0,0]\n\n"
        
        "   SPAWN PLANNING:\n"
        "   • Select meshes from Valid Static Meshes list\n"
        "   • Use descriptive ObjectName (e.g., \"EntryGate_01\", \"CornerTree_02\")\n"
        "   • Set appropriate Scale (default [1,1,1])\n"
        "   • Set Rotation in [Pitch, Yaw, Roll] format\n"
        "   • Set ClearanceRadius for collision avoidance (default: 150.0)\n"
        "   • Set bSpawnActors = true if you add any spawns\n\n"
        
        "   EXAMPLE SEMANTIC LOCATIONS:\n"
        "   • \"PLAYER_FRONT\" - in front of player spawn\n"
        "   • \"PLAYER_BACK\" - behind player spawn\n"
        "   • \"CENTER\" - scene center\n"
        "   • \"NORTH_WALL\", \"SOUTH_WALL\", \"EAST_WALL\", \"WEST_WALL\"\n"
        "   • \"CORNER_NE\", \"CORNER_NW\", \"CORNER_SE\", \"CORNER_SW\"\n"
        "   • \"Arena_Center\", \"Room_01_Center\" - any named zone\n\n"
        
        "5. PARTICLE EFFECTS (OPTIONAL)\n"
        "   • Similar to SpawnRequest but for particle systems\n"
        "   • Use ParticleSpawns array\n"
        "   • Same semantic location system applies\n"
        "   • Smaller ClearanceRadius (default: 50.0)\n\n"
        
        "6. LAYOUT COMMANDS (PROCEDURAL PLACEMENT)\n"
        "   • Use for repetitive patterns (walls, fences, grids)\n"
        "   • Available Tools: FILL_PERIMETER, SCATTER, GRID, RING\n"
        "   • TargetZone: semantic zone name\n"
        "   • Style.MeshKeyword: partial mesh name to search for\n"
        "   • Style.MaterialKeyword: partial material name to search for\n"
        "   • Settings: tool-specific parameters (e.g., {\"Spacing\": 200.0, \"Count\": 10})\n\n"
        
        "   EXAMPLE LAYOUT COMMAND:\n"
        "   {\n"
        "     \"Tool\": \"FILL_PERIMETER\",\n"
        "     \"TargetZone\": \"Arena\",\n"
        "     \"Style\": {\n"
        "       \"MeshKeyword\": \"Wall\",\n"
        "       \"MaterialKeyword\": \"Stone\"\n"
        "     },\n"
        "     \"Settings\": {\"Spacing\": 300.0}\n"
        "   }\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "COMPLETE JSON SCHEMA (FEnhancedScenePlan)\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        
        "{\n"
        "  \"ThemeName\": \"descriptive_theme_name\",\n"
        "  \n"
        "  \"bModifyEnvironment\": true/false,\n"
        "  \"bModifyProps\": true/false,\n"
        "  \"bSpawnActors\": true/false,\n"
        "  \"TargetPropTags\": [],\n"
        "  \n"
        "  \"Environment\": {\n"
        "    \"Lighting\": {\n"
        "      \"SunColor\": [R, G, B],              // floats 0.0-1.0\n"
        "      \"SunIntensity\": 10.0,              // float 0.0-50.0\n"
        "      \"SunPitch\": -45.0,                 // float -90 to 90\n"
        "      \"SunYaw\": 0.0,                     // float 0-360\n"
        "      \"SkyLightColor\": [R, G, B],       // floats 0.0-1.0\n"
        "      \"SkyLightIntensity\": 1.0,         // float 0.0-10.0\n"
        "      \"SunTemperature\": 6500.0,         // float 1000-15000 (optional)\n"
        "      \"bUseTemperature\": false          // boolean\n"
        "    },\n"
        "    \"FogDensity\": 0.1,                  // float 0.0-5.0\n"
        "    \"FogColor\": [R, G, B],              // integers 0-255\n"
        "    \"PostProcessingName\": \"\"           // string from list or empty\n"
        "  },\n"
        "  \n"
        "  \"Props\": [\n"
        "    {\n"
        "      \"TagName\": \"exact_tag_from_list\",        // MUST exist in Valid Actor Tags\n"
        "      \"PropColor\": [R, G, B],                   // integers 0-255\n"
        "      \"Texture\": {\n"
        "        \"BaseColorPath\": \"material_name\",      // MUST exist in Valid Materials\n"
        "        \"NormalPath\": \"\",                      // optional\n"
        "        \"RoughnessPath\": \"\",                   // optional\n"
        "        \"MetallicPath\": \"\",                    // optional\n"
        "        \"AOPath\": \"\",                          // optional\n"
        "        \"DisplacementPath\": \"\",                // optional\n"
        "        \"OpacityPath\": \"\"                      // optional\n"
        "      },\n"
        "      \"ParticleEffects\": \"\"                    // optional particle system\n"
        "    }\n"
        "  ],\n"
        "  \n"
        "  \"SpawnRequest\": [\n"
        "    {\n"
        "      \"AssetPath\": \"exact_mesh_name\",         // MUST exist in Valid Static Meshes\n"
        "      \"ObjectName\": \"unique_instance_name\",   // e.g., \"Tree_01\"\n"
        "      \"SpawnLocation\": [0, 0, 0],              // will be resolved - can leave as [0,0,0]\n"
        "      \"LocationName\": \"SEMANTIC_LOCATION\",    // e.g., \"PLAYER_FRONT\", \"CENTER\"\n"
        "      \"LocationOffset\": [X, Y, Z],             // offset from resolved location\n"
        "      \"Rotation\": [Pitch, Yaw, Roll],          // floats in degrees\n"
        "      \"Scale\": [X, Y, Z],                      // floats (default [1,1,1])\n"
        "      \"Tag\": \"optional_tag\",                  // optional tracking tag\n"
        "      \"ClearanceRadius\": 150.0                 // float - collision avoidance radius\n"
        "    }\n"
        "  ],\n"
        "  \n"
        "  \"ParticleSpawns\": [\n"
        "    {\n"
        "      \"AssetPath\": \"particle_system_name\",\n"
        "      \"ObjectName\": \"unique_particle_name\",\n"
        "      \"SpawnLocation\": [0, 0, 0],\n"
        "      \"LocationName\": \"SEMANTIC_LOCATION\",\n"
        "      \"LocationOffset\": [X, Y, Z],\n"
        "      \"Rotation\": [Pitch, Yaw, Roll],\n"
        "      \"Scale\": [X, Y, Z],\n"
        "      \"Tag\": \"optional_tag\",\n"
        "      \"ClearanceRadius\": 50.0\n"
        "    }\n"
        "  ],\n"
        "  \n"
        "  \"LayoutCommands\": [\n"
        "    {\n"
        "      \"Tool\": \"FILL_PERIMETER\",               // or SCATTER, GRID, RING\n"
        "      \"TargetZone\": \"zone_name\",             // semantic zone identifier\n"
        "      \"Archetype\": \"optional_archetype\",     // optional preset\n"
        "      \"Style\": {\n"
        "        \"MeshKeyword\": \"mesh_search_term\",   // partial name to match\n"
        "        \"MaterialKeyword\": \"mat_search_term\" // partial name to match\n"
        "      },\n"
        "      \"Settings\": {                            // tool-specific parameters\n"
        "        \"Spacing\": 200.0,\n"
        "        \"Count\": 10,\n"
        "        \"Radius\": 500.0\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "CRITICAL VALIDATION RULES\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        
        "ASSET VALIDATION (MANDATORY):\n"
        "✓ Every AssetPath in SpawnRequest MUST exist in Valid Static Meshes\n"
        "✓ Every AssetPath in ParticleSpawns MUST be a valid particle system\n"
        "✓ Every TagName in Props MUST exist in Valid Actor Tags\n"
        "✓ Every BaseColorPath in Texture MUST exist in Valid Materials\n"
        "✓ PostProcessingName MUST be from Valid Post-Process list or empty string\n"
        "✓ NO HALLUCINATION - if asset doesn't exist in lists, DON'T use it\n\n"
        
        "VALUE RANGE VALIDATION:\n"
        "✓ SunColor, SkyLightColor: floats 0.0-1.0\n"
        "✓ PropColor, FogColor: integers 0-255\n"
        "✓ SunIntensity: 0.0-50.0\n"
        "✓ SkyLightIntensity: 0.0-10.0\n"
        "✓ FogDensity: 0.0-5.0\n"
        "✓ SunPitch: -90.0 to 90.0\n"
        "✓ SunYaw: 0.0-360.0\n"
        "✓ Scale: positive floats (typical: 0.5-5.0)\n\n"
        
        "STRUCTURAL VALIDATION:\n"
        "✓ All ObjectName values must be unique\n"
        "✓ Arrays must use proper brackets []\n"
        "✓ Objects must use proper braces {}\n"
        "✓ No trailing commas\n"
        "✓ All strings in quotes\n"
        "✓ All numbers unquoted\n"
        "✓ Booleans as true/false (not strings)\n\n"
        
        "FLAG CONSISTENCY:\n"
        "✓ If Environment has content → bModifyEnvironment = true\n"
        "✓ If Props array is non-empty → bModifyProps = true\n"
        "✓ If SpawnRequest or ParticleSpawns non-empty → bSpawnActors = true\n"
        "✓ If all arrays empty → at least set bModifyEnvironment = true for lighting\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "SMART DESIGN GUIDELINES\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        
        "WHEN CLIENT REQUEST IS VAGUE:\n"
        "• Create a simple, atmospheric scene with good lighting\n"
        "• Use 3-5 key objects that establish the theme\n"
        "• Focus on mood through lighting rather than clutter\n\n"
        
        "WHEN CLIENT REQUEST IS DETAILED:\n"
        "• Match all specified elements\n"
        "• Search asset lists for best thematic matches\n"
        "• Prioritize quality over quantity\n\n"
        
        "LOCATION STRATEGY:\n"
        "• Use semantic names that make spatial sense\n"
        "• Distribute objects using variety (FRONT, BACK, CORNERS)\n"
        "• Use LocationOffset for fine positioning\n"
        "• Trust the resolver to handle exact coordinates\n\n"
        
        "MATERIAL SELECTION:\n"
        "• Match materials to object type (wood for trees, metal for machines)\n"
        "• Use PropColor for subtle variations\n"
        "• Leave optional texture paths empty if not needed\n\n"
        
        "LAYOUT COMMANDS:\n"
        "• Use for repetitive elements (walls, forests, grids)\n"
        "• Keep Settings simple - resolver will handle math\n"
        "• One command can spawn dozens of objects efficiently\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "OUTPUT FORMAT REQUIREMENTS\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        
        "🔴 CRITICAL JSON OUTPUT RULES:\n"
        "1. Return ONLY valid JSON - no markdown, no explanations, no preamble\n"
        "2. Start with '{' and end with '}'\n"
        "3. Do NOT include ```json code fences\n"
        "4. Ensure all arrays and objects are properly closed\n"
        "5. No trailing commas after last elements\n"
        "6. All strings must be in double quotes\n"
        "7. All numeric values must be unquoted\n"
        "8. Booleans must be true/false (not \"true\"/\"false\")\n"
        "9. Empty strings must be \"\" (not null)\n"
        "10. Empty arrays must be [] (not null)\n\n"
        
        "═══════════════════════════════════════════════════════════════\n"
        "FINAL CHECKLIST BEFORE OUTPUT\n"
        "═══════════════════════════════════════════════════════════════\n\n"
        
        "Before submitting your JSON, verify:\n"
        "☐ All asset paths exist in provided lists\n"
        "☐ All numeric values within valid ranges\n"
        "☐ All required fields present\n"
        "☐ Flags (bModify*) set correctly\n"
        "☐ No duplicate ObjectName values\n"
        "☐ Valid JSON syntax (test with a validator if unsure)\n"
        "☐ LocationName uses semantic descriptors\n"
        "☐ Lighting matches requested theme/mood\n\n"
        
        "═══════════════════════════════════════════════════════════════\n\n"
        
        "Your JSON will be directly parsed by C++ into FEnhancedScenePlan.\n"
        "Try to Apply Texture modifications plans to all tags discovered\n"
        "Invalid JSON = system crash. Invalid assets = runtime errors.\n"
        "Precision and validation are critical.\n\n"
        
        "CREATE COMPLETE SCENE PLAN NOW:"
    ),
    *UserPrompt,
    *ValidMeshString,
    *ValidMatString,
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

	// 3. NEW: Recover Painting Data
	if (!DraftPaintingJson.IsEmpty())
	{
		// We use the parser to convert the specialist's JSON into a plan struct
		FEnhancedScenePlan PaintingDraft = UJsonParser::CreatePlan(DraftPaintingJson);
		BackupPlan.LayoutCommands = PaintingDraft.LayoutCommands;
		UE_LOG(LogTemp, Display, TEXT("   + Recovered %d Layout Commands from Draft"), BackupPlan.LayoutCommands.Num());
	}
	// 4. ✅ APPLY SMART RANDOM LIGHTING
	// This makes the fallback feel like a feature, not a bug.
	ApplySmartFallbackLighting(BackupPlan, LastUserPrompt);
	
	// Set flags so the SceneStateTracker knows there is work to do
	BackupPlan.bSpawnActors = (BackupPlan.SpawnRequest.Num() > 0 || BackupPlan.LayoutCommands.Num() > 0);
	BackupPlan.bModifyEnvironment = true; 
	BackupPlan.bModifyProps = (BackupPlan.Props.Num() > 0);
    
	// Broadcast the Frankenstein plan
	OnThemeDataReady.Broadcast(BackupPlan, LastUserPrompt);
}

void UGenAISystem::ApplySmartFallbackLighting(FEnhancedScenePlan& Plan, const FString& Prompt)
{
    Plan.bModifyEnvironment = true;
    FString P = Prompt.ToLower();

    // 1. Define Archetypes
    enum class ELightTheme { Day, Sunset, Night, Cyberpunk, Horror };
    ELightTheme SelectedTheme = ELightTheme::Day;

    // 2. Keyword Detection (Smart Selection)
    if (P.Contains("night") || P.Contains("dark") || P.Contains("moon")) 
        SelectedTheme = ELightTheme::Night;
    else if (P.Contains("sunset") || P.Contains("evening") || P.Contains("dusk")) 
        SelectedTheme = ELightTheme::Sunset;
    else if (P.Contains("cyber") || P.Contains("neon") || P.Contains("future")) 
        SelectedTheme = ELightTheme::Cyberpunk;
    else if (P.Contains("horror") || P.Contains("scary") || P.Contains("spooky")) 
        SelectedTheme = ELightTheme::Horror;
    else 
    {
        // 3. Random Fallback (If prompt is vague like "make a scene")
        // Pick a random theme to keep it interesting
        int32 Roll = FMath::RandRange(0, 3);
        if (Roll == 0) SelectedTheme = ELightTheme::Day;
        if (Roll == 1) SelectedTheme = ELightTheme::Sunset;
        if (Roll == 2) SelectedTheme = ELightTheme::Night;
        if (Roll == 3) SelectedTheme = ELightTheme::Horror;
    }

    // 4. Apply Thematics
    switch (SelectedTheme)
    {
    case ELightTheme::Day:
        Plan.Environment.Lighting.SunColor = FLinearColor(1.0f, 0.95f, 0.8f);
        Plan.Environment.Lighting.SunIntensity = FMath::RandRange(8.0f, 12.0f); // Slight variance
        Plan.Environment.Lighting.SunPitch = FMath::RandRange(-60.0f, -45.0f); // High noonish
        Plan.Environment.Lighting.SunYaw = FMath::RandRange(0.0f, 360.0f);
        Plan.Environment.Lighting.SkyLightIntensity = 1.0f;
        Plan.Environment.FogDensity = 0.01f;
        Plan.Environment.FogColor = FColor(200, 220, 255);
        break;

    case ELightTheme::Sunset:
        Plan.Environment.Lighting.SunColor = FLinearColor(1.0f, 0.5f, 0.2f); // Orange
        Plan.Environment.Lighting.SunIntensity = 20.0f; // Bright sun disk
        Plan.Environment.Lighting.SunPitch = FMath::RandRange(-15.0f, -5.0f); // Low angle
        Plan.Environment.Lighting.SunYaw = FMath::RandRange(0.0f, 360.0f);
        Plan.Environment.Lighting.SkyLightIntensity = 0.7f;
        Plan.Environment.FogDensity = 0.2f; // Haze
        Plan.Environment.FogColor = FColor(255, 180, 120);
        break;

    case ELightTheme::Night:
        Plan.Environment.Lighting.SunColor = FLinearColor(0.6f, 0.7f, 1.0f); // Moonlight blue
        Plan.Environment.Lighting.SunIntensity = 0.5f; // Dim
        Plan.Environment.Lighting.SunPitch = -45.0f;
        Plan.Environment.Lighting.SkyLightIntensity = 0.2f;
        Plan.Environment.FogDensity = 0.5f;
        Plan.Environment.FogColor = FColor(10, 10, 20); // Black/Blue fog
        break;

    case ELightTheme::Cyberpunk:
        Plan.Environment.Lighting.SunColor = FLinearColor(0.0f, 0.0f, 0.0f); // No sun, just city lights
        Plan.Environment.Lighting.SunIntensity = 0.0f;
        Plan.Environment.Lighting.SkyLightIntensity = 0.5f;
        Plan.Environment.Lighting.SkyLightColor = FLinearColor(0.2f, 0.0f, 0.5f); // Purple ambient
        Plan.Environment.FogDensity = 1.5f; // Heavy smog
        Plan.Environment.FogColor = FColor(40, 0, 60); // Purple fog
        break;

    case ELightTheme::Horror:
        Plan.Environment.Lighting.SunColor = FLinearColor(0.3f, 0.35f, 0.4f); // Dead grey
        Plan.Environment.Lighting.SunIntensity = 2.0f;
        Plan.Environment.Lighting.SunPitch = -20.0f;
        Plan.Environment.Lighting.SkyLightIntensity = 0.1f; // Very dark shadows
        Plan.Environment.FogDensity = 2.0f; // Thick fog
        Plan.Environment.FogColor = FColor(50, 50, 50); // Grey fog
        break;
    }

    UE_LOG(LogTemp, Display, TEXT("🎨 GenAI: Applied Fallback Lighting Theme: %d"), (int32)SelectedTheme);
}

bool UGenAISystem::IsAuthorized() 
{
	// 1. If I'm not the designated Alpha, I am not authorized.
	if (GlobalActiveAuthority.Get() != this) return false;
    
	// 2. If my ID doesn't match the latest global sequence, I am a 'Ghost'.
	if (MyActiveRequestID != GlobalSequenceNonce) return false;

	return true;
}

void UGenAISystem::ResetPipeline()
{
	bIsGlobalPipelineBusy = false;
	bIsMeshReady = false;
	bIsTexReady = false;
	bIsPaintingReady = false;
	bIsIntentReady = false;
    
	// Clear the drafts to prevent data leaking into the next prompt
	DraftMeshJson = "";
	DraftTexJson = "";
	DraftPaintingJson = "";
}
