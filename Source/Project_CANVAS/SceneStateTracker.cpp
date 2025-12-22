// Fill out your copyright notice in the Description page of Project Settings.

#include "SceneStateTracker.h"
#include "GenAISystem.h"
#include "SceneBuilder.h"
#include "SceneHistoryManager.h"
#include"ScenePlan.h"
#include"LocationQueryEngine.h"
#include "API_KEY.h"
#include "NiagaraComponent.h"
void USceneStateTracker::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize( Collection);

    // 2. LATCH: Ensure we don't run twice for the same game session.
    static bool bIsLogicActive = false;
    if (bIsLogicActive) return;
    bIsLogicActive = true;
    
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("╔═══════════════════════════════════════════╗"));
    UE_LOG(LogTemp, Warning, TEXT("║  🚀 SceneStateTracker::Init()             ║"));
    UE_LOG(LogTemp, Warning, TEXT("╚═══════════════════════════════════════════╝"));

    // ✅ FIX: Separate assignment from condition
    AssetIndexer = NewObject<UAssetIndexer>(this);
    if (!AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ INIT FAILED: AssetIndexer creation failed"));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("✅ [1/5] AssetIndexer"));

    // ✅ FIX: Separate assignment from condition
    LocationEngine = NewObject<ULocationQueryEngine>(this);
    if (!LocationEngine)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ INIT FAILED: LocationEngine creation failed"));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("✅ [2/5] LocationEngine"));

    // ✅ FIX: Separate assignment from condition
    GenAISystem = NewObject<UGenAISystem>(this);
    if (!GenAISystem)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ INIT FAILED: GenAISystem creation failed"));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("✅ [3/5] GenAISystem"));

    // ✅ FIX: Separate assignment from condition
    SceneBuilder = NewObject<USceneBuilder>(this);
    if (!SceneBuilder)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ INIT FAILED: SceneBuilder creation failed"));
        return;
    }
    SceneBuilder->StateTracker = this;
    UE_LOG(LogTemp, Warning, TEXT("✅ [4/5] SceneBuilder"));

    // ✅ FIX: Separate assignment from condition
    HistoryManager = NewObject<USceneHistoryManager>(this);
    if (!HistoryManager)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ INIT FAILED: HistoryManager creation failed"));
        return;
    }
    UE_LOG(LogTemp, Warning, TEXT("✅ [5/5] HistoryManager"));

    // Bind delegates
    if (AssetIndexer && AssetIndexer->OnScanComplete.IsBound() == false)
    {
        AssetIndexer->OnScanComplete.AddDynamic(this, &USceneStateTracker::OnAssetScanFinished);
    }
    
    if (GenAISystem && GenAISystem->OnThemeDataReady.IsBound() == false)
    {
        GenAISystem->OnThemeDataReady.AddDynamic(this, &USceneStateTracker::OnPlanReceived);
    }
    
    UE_LOG(LogTemp, Display, TEXT("📌 Delegates bound"));

    // Start scans
    if (GetWorld())
    {
        if (AssetIndexer)
        {
            AssetIndexer->ScanAllAssetsAsync(GetWorld());
        }
        
        if (LocationEngine)
        {
            LocationEngine->InitializePlayableAreaBounds();
            LocationEngine->ScanWorldLocationsAsync(GetWorld());
            // ✅ UPDATE: Configure Location Resolver to use Gemini 1.5 Flash
            API_KEY KeyHandler;
            FString GeminiKey = KeyHandler.GetGeminiKey(); // Ensure API_KEY class has this!
            
            // Gemini uses key in URL. Leave APIKey param empty.
            FString Endpoint = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%s"), *GeminiKey);
            
            LocationEngine->ConfigureLLMFallback(
                Endpoint,
                TEXT(""), // No Auth Header for Gemini
                TEXT("gemini-2.5-flash")
            );

        }
        
        UE_LOG(LogTemp, Display, TEXT("🔄 Async scans started"));
    }
    else
    {
        if (AssetIndexer)
        {
            AssetIndexer->ScanForTexturesAsync(TEXT("/Game/DATABASE/textures"));
            AssetIndexer->ScanForStaticMeshesAsync(TEXT("/Game/DATABASE/meshes"));
            AssetIndexer->ScanForParticlesAsync(TEXT("/Game/DATABASE/particles"));
        }
        UE_LOG(LogTemp, Warning, TEXT("⚠️  World unavailable - asset-only scan"));
    }

  
    ActorNameCounter = 0;
    bAssetScanComplete = false;
    bLocationScanComplete = false;

    UE_LOG(LogTemp, Warning, TEXT("✅ Init complete - waiting for scans"));
    UE_LOG(LogTemp, Warning, TEXT(""));

    FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &USceneStateTracker::OnWorldInit);
}

void USceneStateTracker::OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
  //  Super::OnStart();
    
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("🚀 SceneStateTracker::OnStart"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    
    // ✅ World is now GUARANTEED to be available
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ CRITICAL: World is STILL null in OnStart()!"));
        return;
    }
    
    // ✅ Initialize bounds NOW (world is loaded)
    if (LocationEngine)
    {
     
      LocationEngine->InitializePlayableAreaBounds();
        if (LocationEngine->IsBoundsInitialized())
        {
            UE_LOG(LogTemp, Display, TEXT("✅ Bounds initialized: %s"), 
                *LocationEngine->GetPlayableAreaBounds().ToString());
            
            // ✅ Show bounds for debugging (optional)
            LocationEngine->VisualizePlayableAreaBounds(15.0f);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("❌ Bounds initialization FAILED!"));
        }
        
        // ✅ Re-scan locations with bounds now active
        LocationEngine->ScanWorldLocationsAsync(World);
    }
    
    // ✅ Re-scan assets if Init() couldn't access world
    if (AssetIndexer && !bAssetScanComplete)
    {
        UE_LOG(LogTemp, Display, TEXT("🔄 Retrying asset scan with world access..."));
        AssetIndexer->ScanAllAssetsAsync(World);
    }

    GenAISystem->Initialize();

    UE_LOG(LogTemp, Display, TEXT("✅ OnStart complete - system fully initialized"));
    World->GetTimerManager().SetTimer(
        InitTimerHandle, 
        this, 
        &USceneStateTracker::DelayedInit, 
        1.0f, // 1 Second Delay
        false
    );
}

void USceneStateTracker::Deinitialize()
{
   
    Super::Deinitialize();
    GenAISystem->Deinitialize();
    FWorldDelegates::OnPostWorldInitialization.RemoveAll(this);
}
void USceneStateTracker::OnAssetScanFinished()
{
    bAssetScanComplete = true;

    TArray<FString> Textures = AssetIndexer->GetDiscoveredTextureNames();
    TArray<FString> Meshes = AssetIndexer->GetAllMeshNames();
    TArray<FString> Particles = AssetIndexer->GetDiscoveredParticleNames();

    UE_LOG(LogTemp, Warning, TEXT("✅ AssetScan: %d textures, %d meshes, %d particles"),
        Textures.Num(), Meshes.Num(), Particles.Num());

    CheckSystemsReady();
}

void USceneStateTracker::OnLocationScanFinished()
{
    bLocationScanComplete = true;

    // Get all discovered spawn locations
    TArray<FSpawnLocation> Locations = LocationEngine->GetAllLocations();
    
    UE_LOG(LogTemp, Warning, TEXT("✅ LocationScan: %d spawn points"), Locations.Num());
    
    // Optional: Print detailed location information for debugging
    LocationEngine->PrintAllLocationData();

    // Check if all systems are ready
    CheckSystemsReady();
}


void USceneStateTracker::CheckSystemsReady()
{
    if (bAssetScanComplete && bLocationScanComplete)
    {
        UE_LOG(LogTemp, Warning, TEXT(""));
        UE_LOG(LogTemp, Warning, TEXT("╔═══════════════════════════════════════════╗"));
        UE_LOG(LogTemp, Warning, TEXT("║ ✅ ALL SYSTEMS READY - GenAI Enabled    ║"));
        UE_LOG(LogTemp, Warning, TEXT("╚═══════════════════════════════════════════╝"));
        UE_LOG(LogTemp, Warning, TEXT(""));
    }
}

void USceneStateTracker::OnAssetsLoaded()
{
    UE_LOG(LogTemp, Display, TEXT("🔄 Phase 3: Assets Loaded! Finalizing Build..."));

    // ---------------------------------------------------------
    // STEP 4: CLAMPING (REQUIRES LOADED ASSETS)
    // ---------------------------------------------------------
    // We moved this here because we can't check bounds until the mesh is in memory.
    for (FSpawnRequest& Req : PendingPlan.SpawnRequest)
    {
        if (Req.AssetPath.IsEmpty()) continue;

        // StaticLoadObject is now instant/safe because the asset is already in RAM.
        UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Req.AssetPath));
        
        if (Mesh)
        {
            FBoxSphereBounds Bounds = Mesh->GetBounds();
            float RealSize = Bounds.BoxExtent.GetMax() * 2.0f;
            
            // Your original clamping logic
            if (RealSize > 2000.0f && Req.Scale.X >= 1.0f)
            {
                UE_LOG(LogTemp, Warning, TEXT("📉 Clamping giant mesh '%s' (Size: %.0f). Scale Reset to 1.0"), 
                    *Req.ObjectName, RealSize);
                Req.Scale = FVector::OneVector;
            }
        }
    }

    // ---------------------------------------------------------
    // STEP 5: EXECUTE BUILD
    // ---------------------------------------------------------
    if (SceneBuilder)
    {
        UE_LOG(LogTemp, Warning, TEXT("🎬 Triggering SceneBuilder with %d actors"), PendingPlan.SpawnRequest.Num());
        SceneBuilder->BuildScene(PendingPlan, GetWorld());
    }
    
    // ---------------------------------------------------------
    // STEP 6: HISTORY & CLEANUP
    // ---------------------------------------------------------
    if (HistoryManager)
    {
        HistoryManager->SavePlan(PendingPlan, "Finalized Build");
    }
    
    PendingPlan = FEnhancedScenePlan();
    UE_LOG(LogTemp, Warning, TEXT("✅ Plan execution complete"));
    UE_LOG(LogTemp, Warning, TEXT(""));
    // ✅ FINALLY release the lock here, after the world is actually built
    if (GenAISystem)
    GenAISystem->ResetPipeline(); 
    UE_LOG(LogTemp, Warning, TEXT("✅ Build Complete: Pipeline Unlocked."));
    UE_LOG(LogTemp, Display, TEXT("🔓 GenAI: Pipeline Released. Ready for next command."));
}


void USceneStateTracker::OnPlanReceived(const FEnhancedScenePlan& Plan, const FString& UserPrompt)
{
    if (!GetWorld())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ OnPlanReceived FAILED: Invalid world"));
        return;
    }
    if (!LocationEngine || !SceneBuilder)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Critical subsystems not ready - skipping plan"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("╔═══════════════════════════════════════════╗"));
    UE_LOG(LogTemp, Warning, TEXT("║  📥 Plan Received - %d props, %d spawn    ║"), 
        Plan.Props.Num(), Plan.SpawnRequest.Num());
    UE_LOG(LogTemp, Warning, TEXT("╚═══════════════════════════════════════════╝"));

    // 1. Lazy Initialize Bounds (The Fix for "No tagged geometry")
    // We do this here to ensure the level is fully loaded before we measure it.
    if (LocationEngine && !LocationEngine->IsBoundsInitialized())
    {
        UE_LOG(LogTemp, Warning, TEXT("🔄 Lazy-Initializing Location Bounds..."));
        LocationEngine->InitializePlayableAreaBounds();
        LocationEngine->ScanWorldLocationsAsync(GetWorld());
    }

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("╔═══════════════════════════════════════════╗"));
    UE_LOG(LogTemp, Warning, TEXT("║  📥 Plan Received - Orchestrating Build   ║"));
    UE_LOG(LogTemp, Warning, TEXT("╚═══════════════════════════════════════════╝"));

    // 1. Reset Scene
    ClearAllGeneratedContent();
    
    // Copy plan for modification
    FEnhancedScenePlan EnrichedPlan = Plan;

    // 2. DELEGATE ASSET RESOLUTION (The new clean part)
    if (AssetIndexer)
    {
        AssetIndexer->BatchResolveTextures(EnrichedPlan);
        AssetIndexer->BatchResolveMeshes(EnrichedPlan);
        AssetIndexer->BatchResolveParticles(EnrichedPlan);
        AssetIndexer->ResolveEnvironmentAssets(EnrichedPlan);
    }

    ResolveProceduralLayouts(EnrichedPlan);
    // 3. SPATIAL RESOLUTION (The part you are keeping here for batching logic)
    if (EnrichedPlan.bSpawnActors && EnrichedPlan.SpawnRequest.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("🔄 [3/3] ResolveLocationsInPlan (Async)..."));
        
        // This function stays in SceneStateTracker for now because it handles
        // the PendingPlan state and the HTTP batch callback.
        ResolveLocationsInPlan(EnrichedPlan); 
        return; 
    }

    // 4. ENVIRONMENT ONLY PATH
    if (SceneBuilder)
    {
        SceneBuilder->BuildScene(EnrichedPlan, GetWorld());
    }

    if (HistoryManager)
    {
        HistoryManager->SavePlan(EnrichedPlan, UserPrompt);
    }

    LogSceneStateVerbose();
    UE_LOG(LogTemp, Warning, TEXT("✅ Plan execution complete (Environment Only)"));
    UE_LOG(LogTemp, Warning, TEXT(""));
}



void USceneStateTracker::ResolveLocationsInPlan(FEnhancedScenePlan& Plan)
{
    if (!LocationEngine || !LocationEngine->GetLLMResolver())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ResolveLocationsInPlan FAILED: Components missing"));
        return;
    }

    // Store Plan Globally
    PendingPlan = Plan; 
    TArray<FSpawnRequest> BatchRequests;

    UE_LOG(LogTemp, Display, TEXT("🔄 Phase 1: Separating Local vs Remote requests..."));

    for (int32 i = 0; i < PendingPlan.SpawnRequest.Num(); i++)
    {
        FSpawnRequest& Req = PendingPlan.SpawnRequest[i];

        // Try Local Resolution (Fast)
        FVector LocalResult = LocationEngine->ResolveLocationName(Req.LocationName);

        if (!LocalResult.IsZero())
        {
            Req.SpawnLocation = LocalResult;
            LocationEngine->SetLocationOccupied(Req.LocationName, true);
            UE_LOG(LogTemp, Display, TEXT("   ✅ Local Resolve: %s -> %s"), *Req.ObjectName, *LocalResult.ToString());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("   ⚠️ Local Failed for '%s'. Queueing for Batch AI."), *Req.ObjectName);
            BatchRequests.Add(Req);
        }
    }
// particles locations
    for (int32 i = 0; i < PendingPlan.ParticleSpawns.Num(); i++)
    {
        FSpawnRequest& Req = PendingPlan.ParticleSpawns[i];
        FVector LocalResult = LocationEngine->ResolveLocationName(Req.LocationName);

        if (!LocalResult.IsZero())
        {
            Req.SpawnLocation = LocalResult;
            // Note: We usually DON'T mark location occupied for particles so they can overlap meshes
            // LocationEngine->SetLocationOccupied(Req.LocationName, true); 
        }
        else
        {
            BatchRequests.Add(Req); // Add to same batch for AI resolution
        }
    }
    // The Fork
    if (BatchRequests.Num() == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("🚀 All locations resolved locally. Building immediately."));
        FinalizeSceneBuild(TMap<FString, FResolutionResult>()); 
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("⏳ Sending Async Batch for %d items... Game continues."), BatchRequests.Num());
        
        LocationEngine->GetLLMResolver()->ResolveBatchLocationsAsync(
            BatchRequests,
            LocationEngine->GetLocationContextForLLM(),
            FOnBatchLocationsResolved::CreateUObject(this, &USceneStateTracker::FinalizeSceneBuild)
        );
    }
}

void USceneStateTracker::ResolveEnvironmentAssets(FEnhancedScenePlan& Plan)
{
    if (!AssetIndexer || Plan.Environment.PostProcessingName.IsEmpty()) return;

    FString FullPath = AssetIndexer->ResolvePostProcessPath(Plan.Environment.PostProcessingName);
    
    if (!FullPath.IsEmpty())
    {
        Plan.Environment.PostProcessingName = FullPath;
    }
}

void USceneStateTracker::FinalizeSceneBuild(const TMap<FString, FResolutionResult>& AsyncResults)
{
    UE_LOG(LogTemp, Display, TEXT("🔄 Phase 2: Starting Intelligence Bridge (Async)..."));

    // 1. DATA SAFETY: Create a local copy of the plan for the background task.
    // This prevents "Data Races" if a user sends a new prompt while this is running.
    FEnhancedScenePlan TaskPlan = PendingPlan; 
    bool bApiFailed = (AsyncResults.Num() == 0);

    // ---------------------------------------------------------
    // STEP A: BACKGROUND THREAD - HEAVY DATA RESOLUTION
    // ---------------------------------------------------------
    // We resolve strings, keywords, and material sets on a worker thread.
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [this, TaskPlan, AsyncResults, bApiFailed]() mutable
    {
        // 1. Resolve Mesh Keywords to Real Paths
        for (FSpawnRequest& Req : TaskPlan.SpawnRequest)
        {
            FString ResolvedPath = AssetIndexer->ResolveMeshToFullPathWithVariants(Req.AssetPath);
            if (!ResolvedPath.IsEmpty()) Req.AssetPath = ResolvedPath;
        }

        // 2. Resolve Texture Keywords to Real Material Sets
        for (FPropsModification& Prop : TaskPlan.Props)
        {
            FTextureSet ResolvedSet = AssetIndexer->ResolveTextureFromName(Prop.Texture.BaseColorPath);
            if (!ResolvedSet.BaseColorPath.IsEmpty()) Prop.Texture = ResolvedSet;
        }

        // ---------------------------------------------------------
        // STEP B: GAME THREAD - PHYSICS & SPATIAL LOGIC
        // ---------------------------------------------------------
        // We jump back to the Game Thread because World/Physics queries are NOT thread-safe.
        AsyncTask(ENamedThreads::GameThread, [this, TaskPlan, AsyncResults, bApiFailed]() mutable
        {
            // Iterate backwards so we can safely remove items converted to patterns
            for (int32 i = TaskPlan.SpawnRequest.Num() - 1; i >= 0; i--)
            {
                FSpawnRequest& Req = TaskPlan.SpawnRequest[i];

                if (!bApiFailed && AsyncResults.Contains(Req.ObjectName))
                {
                    const FResolutionResult& Res = AsyncResults[Req.ObjectName];

                    // ✅ ALGORITHMIC DELEGATION: Check if AI chose a pattern (Circle, Grid, etc.)
                    // PatternID: 1=Circle, 2=Grid, 3=Scatter
                    if (Res.PatternID > 0)
                    {
                        // 1. Convert the AI's "Pattern Choice" into a Layout Command
                        FPaintingCommand AIChosenPattern;
                        AIChosenPattern.Tool = (Res.PatternID == 1) ? TEXT("CIRCLE") : (Res.PatternID == 2) ? TEXT("GRID") : TEXT("SCATTER");
                        AIChosenPattern.TargetZone = Req.LocationName;
                        AIChosenPattern.Style.MeshKeyword = Req.AssetPath; 
                        AIChosenPattern.Settings.Add(TEXT("Radius"), Res.PatternRadius);
                        AIChosenPattern.Settings.Add(TEXT("Count"), 8.0f); // Default for AI patterns
    
                        // 2. Add it to the plan
                        TaskPlan.LayoutCommands.Add(AIChosenPattern);
    
                        // 3. Remove the single spawn so we don't have a duplicate at the center
                        TaskPlan.SpawnRequest.RemoveAt(i); 
                        continue;
                    }

                    // Apply AI Coordinates for standard single placement
                    Req.SpawnLocation = Res.Location;
                    Req.Rotation = FRotator(0, Res.RotationYaw, 0);
                    if (Res.Scale > 0.1f) Req.Scale = FVector(Res.Scale);
                }
                
                // ✅ PHYSICS FALLBACK: Safe here on Game Thread
                if (Req.SpawnLocation.IsNearlyZero())
                {
                    FSpawnLocation Zone = LocationEngine->FindValidSpawnLocation(Req.LocationName, Req.ClearanceRadius);
                    Req.SpawnLocation = Zone.WorldPosition;

                    // Add Jitter/Rotation to prevent stacking
                    float Jitter = FMath::Max(Req.ClearanceRadius, 200.0f);
                    Req.SpawnLocation.X += FMath::RandRange(-Jitter, Jitter);
                    Req.SpawnLocation.Y += FMath::RandRange(-Jitter, Jitter);
                    Req.Rotation.Yaw = FMath::RandRange(0.f, 360.f);
                }

                LocationEngine->SetLocationOccupied(Req.LocationName, true);
            }

            // Calculate the procedural locations for the new LayoutCommands
            ResolveProceduralLayouts(TaskPlan);

            // ---------------------------------------------------------
            // STEP C: ASSET GATHERING & FINAL LOAD
            // ---------------------------------------------------------
            TArray<FSoftObjectPath> AssetsToLoad;
            
            // Gather Meshes
            for (const FSpawnRequest& Req : TaskPlan.SpawnRequest)
            {
                if (Req.AssetPath.StartsWith(TEXT("/"))) AssetsToLoad.Add(FSoftObjectPath(Req.AssetPath));
            }

            // Gather Layout Mesh Paths
            for (const FPaintingCommand& Cmd : TaskPlan.LayoutCommands)
            {
                if (Cmd.ResolvedMeshPath.StartsWith(TEXT("/"))) AssetsToLoad.Add(FSoftObjectPath(Cmd.ResolvedMeshPath));
            }

            // Gather Texture Sets
            for (const FPropsModification& Prop : TaskPlan.Props)
            {
                auto AddIfValid = [&](const FString& Path) { if (Path.StartsWith(TEXT("/"))) AssetsToLoad.Add(FSoftObjectPath(Path)); };
                AddIfValid(Prop.Texture.BaseColorPath);
                AddIfValid(Prop.Texture.NormalPath);
                AddIfValid(Prop.Texture.RoughnessPath);
            }

            // Finalize global state and trigger the async loader
            PendingPlan = TaskPlan;

            if (AssetsToLoad.Num() > 0 && AssetIndexer)
            {
                UE_LOG(LogTemp, Warning, TEXT("⏳ Phase 2 Complete. Loading %d Assets..."), AssetsToLoad.Num());
                AssetIndexer->RequestAsyncLoad(AssetsToLoad, 
                    FStreamableDelegate::CreateUObject(this, &USceneStateTracker::OnAssetsLoaded));
            }
            else
            {
                OnAssetsLoaded();
            }
        });
    });
}

void USceneStateTracker::OnActorSpawned(AActor* NewActor, const FString& ObjectName)
{
    if (!NewActor)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ OnActorSpawned FAILED: Null actor pointer"));
        return;
    }

    SpawnedActors.Add(NewActor);
    SpawnedActorsByName.Add(ObjectName, NewActor);
    ActorToNameMap.Add(NewActor, ObjectName);

    FVector Loc = NewActor->GetActorLocation();
    UE_LOG(LogTemp, Warning, TEXT("🎬 Actor spawned: %s at [%.0f, %.0f, %.0f] | Total: %d"),
        *ObjectName, Loc.X, Loc.Y, Loc.Z, SpawnedActors.Num());
}

void USceneStateTracker::ResolveProceduralLayouts(FEnhancedScenePlan& Plan)
{
    // Safety Checks
    if (!LocationEngine || !AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ResolveProceduralLayouts Failed: Missing Engine or Indexer"));
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("📐 Pre-calculating %d Layout Commands..."), Plan.LayoutCommands.Num());

    for (FPaintingCommand& Cmd : Plan.LayoutCommands)
    {
        // =========================================================
        // 1. ASSET RESOLUTION (Late Binding)
        // =========================================================
        // Convert abstract keyword (e.g., "SciFi_Wall") to concrete path (e.g., "/Game/Meshes/SM_SciFi_Wall_01.uasset")
        // NOTE: AssetIndexer needs ResolveMeshPath/ResolveMaterialPath implemented.
        // ✅ Correct Mesh Resolver
        Cmd.ResolvedMeshPath = AssetIndexer->ResolveMeshToFullPathWithVariants(Cmd.Style.MeshKeyword);
        
        // ✅ Correct Texture Resolver (Returns struct, not string)
        Cmd.ResolvedTextureSet = AssetIndexer->ResolveTextureFromName(Cmd.Style.MaterialKeyword);
        if (Cmd.ResolvedMeshPath.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Could not resolve mesh for keyword '%s'. Skipping command."), *Cmd.Style.MeshKeyword);
            continue;
        }

        // =========================================================
        // 2. SPATIAL RESOLUTION (Math Phase)
        // =========================================================
        
        // Resolve the "Center of Operations" for this command
        FVector Origin = LocationEngine->ResolveLocationName(Cmd.TargetZone);
        
        // Fallback: If "SecretBase" isn't a known location, pick a random safe spot in the center
        if (Origin.IsZero() && !Cmd.TargetZone.Equals("CENTER"))
        {
             UE_LOG(LogTemp, Warning, TEXT("   TargetZone '%s' unknown. Using Random Center fallback."), *Cmd.TargetZone);
             Origin = LocationEngine->GetRandomCenterPosition();
        }

        FString Tool = Cmd.Tool.ToUpper();
        TArray<FTransform>& OutPoints = Cmd.BakedTransforms; // Direct reference to storage

        // ---------------------------------------------------------
        // TOOL A: SCATTER / CLUSTER (Random Organic Placement)
        // ---------------------------------------------------------
        if (Tool.Contains("SCATTER") || Tool.Contains("CLUSTER"))
        {
            int32 Count = (int32)Cmd.Settings.FindRef("Count");
            float Radius = Cmd.Settings.FindRef("Radius");
            float Clearance = Cmd.Settings.FindRef("MinClearance");
            
            // Robust Defaults
            if (Radius <= 0) Radius = 500.0f;
            if (Clearance <= 0) Clearance = 100.0f;
            if (Count <= 0) Count = 5;

            for(int i=0; i<Count; i++)
            {
                // Math: Random point in circle (2D)
                FVector2D RandPt = FMath::RandPointInCircle(Radius);
                FVector Candidate = Origin + FVector(RandPt.X, RandPt.Y, 0);
                
                // Logic: Find safe spot near candidate using Spatial Grid
                // This ensures we don't spawn inside existing walls
                FSpawnLocation SafeLoc = LocationEngine->FindNearestFreeLocation(Candidate, Clearance);
                
                // Logic: Raycast down to floor to handle terrain height
                FVector SnappedPos = LocationEngine->SnapToGround(SafeLoc.WorldPosition);
                
                // Logic: Random Rotation (Yaw) for organic look
                FRotator Rot(0, FMath::RandRange(0.0f, 360.0f), 0);
                
                OutPoints.Add(FTransform(Rot, SnappedPos));
            }
        }
        // ---------------------------------------------------------
        // TOOL B: PERIMETER / WALLS (Boundary Definition)
        // ---------------------------------------------------------
        else if (Tool.Contains("PERIMETER") || Tool.Contains("FILL_PERIMETER"))
        {
            FBox Bounds = LocationEngine->GetPlayableAreaBounds();
            float Spacing = Cmd.Settings.FindRef("Spacing");
            if (Spacing <= 10.0f) Spacing = 400.0f; // Default wall segment size

            // Lambda: Fills a straight line with objects
            auto FillLine = [&](FVector Start, FVector End, float Yaw)
            {
                float Dist = FVector::Dist(Start, End);
                int32 Steps = FMath::FloorToInt(Dist / Spacing);
                if (Steps < 1) Steps = 1;
                
                FVector Dir = (End - Start).GetSafeNormal();
                
                for(int i=0; i<=Steps; i++)
                {
                    FVector Pos = Start + (Dir * (i * Spacing));
                    
                    // Raycast to ensure wall sits on floor
                    Pos = LocationEngine->SnapToGround(Pos);
                    
                    OutPoints.Add(FTransform(FRotator(0, Yaw, 0), Pos));
                }
            };

            // Calculate corners of the arena based on PlayableAreaBounds
            FVector BL(Bounds.Min.X, Bounds.Min.Y, 0);
            FVector BR(Bounds.Min.X, Bounds.Max.Y, 0);
            FVector TL(Bounds.Max.X, Bounds.Min.Y, 0);
            FVector TR(Bounds.Max.X, Bounds.Max.Y, 0);

            // Execute lines (Walls facing appropriately based on Yaw)
            FillLine(BL, TL, 0.0f);   // Left Wall
            FillLine(TL, TR, 90.0f);  // Top Wall
            FillLine(TR, BR, 180.0f); // Right Wall
            FillLine(BR, BL, 270.0f); // Bottom Wall
        }
        // ---------------------------------------------------------
        // TOOL C: GRID (Ordered Placement)
        // ---------------------------------------------------------
        else if (Tool.Contains("GRID"))
        {
            int32 Rows = (int32)Cmd.Settings.FindRef("Rows");
            int32 Cols = (int32)Cmd.Settings.FindRef("Cols");
            float Spacing = Cmd.Settings.FindRef("Spacing");
            
            // Defaults
            if (Rows <= 0) Rows = 3;
            if (Cols <= 0) Cols = 3;
            if (Spacing <= 0) Spacing = 200.0f;

            // Calculate Grid Dimensions
            float TotalWidth = (Cols - 1) * Spacing;
            float TotalDepth = (Rows - 1) * Spacing;
            
            // Start top-left relative to origin so the grid is centered
            FVector StartPos = Origin - FVector(TotalDepth/2, TotalWidth/2, 0);

            for (int r = 0; r < Rows; r++)
            {
                for (int c = 0; c < Cols; c++)
                {
                    FVector Pos = StartPos + FVector(r * Spacing, c * Spacing, 0);
                    
                    // Snap to ground
                    FVector Snapped = LocationEngine->SnapToGround(Pos);
                    
                    // No random rotation for grids (uniform look)
                    OutPoints.Add(FTransform(FRotator::ZeroRotator, Snapped));
                }
            }
        }
        // ---------------------------------------------------------
        // TOOL D: RING / CIRCLE (Ritual Placement)
        // ---------------------------------------------------------
        else if (Tool.Contains("RING") || Tool.Contains("CIRCLE"))
        {
            int32 Count = (int32)Cmd.Settings.FindRef("Count");
            float Radius = Cmd.Settings.FindRef("Radius");
            
            if (Count < 3) Count = 5;
            if (Radius <= 0) Radius = 300.0f;

            for(int i=0; i<Count; i++)
            {
                float Angle = (360.0f / Count) * i;
                float Rad = FMath::DegreesToRadians(Angle);
                
                FVector Offset(FMath::Cos(Rad) * Radius, FMath::Sin(Rad) * Radius, 0);
                FVector Pos = Origin + Offset;
                
                FVector Snapped = LocationEngine->SnapToGround(Pos);
                
                // Rotate to face outward from center
                FRotator Rot(0, Angle, 0); 
                OutPoints.Add(FTransform(Rot, Snapped));
            }
        }
        
        UE_LOG(LogTemp, Display, TEXT("   ✅ Processed '%s': Generated %d points"), *Tool, OutPoints.Num());
    }
}

void USceneStateTracker::PrintAllSpawnedActors() const
{
    if (SpawnedActors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("📋 No spawned actors"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("📋 Spawned Actors (%d)"), SpawnedActors.Num());

    for (int32 i = 0; i < SpawnedActors.Num(); ++i)
    {
        AActor* Actor = SpawnedActors[i];
        if (Actor)
        {
            FVector Loc = Actor->GetActorLocation();
            FString Name = ActorToNameMap.Contains(Actor) ? ActorToNameMap[Actor] : TEXT("Unknown");
            UE_LOG(LogTemp, Display, TEXT("  [%d] %s at [%.0f, %.0f, %.0f]"),
                i + 1, *Name, Loc.X, Loc.Y, Loc.Z);
        }
    }
    UE_LOG(LogTemp, Warning, TEXT(""));
}

AActor* USceneStateTracker::GetSpawnedActorByName(const FString& ObjectName) const
{
    if (!SpawnedActorsByName.Contains(ObjectName))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  GetSpawnedActorByName FAILED: '%s' not found"), *ObjectName);
        return nullptr;
    }

    AActor* Found = SpawnedActorsByName[ObjectName];
    if (!Found)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GetSpawnedActorByName FAILED: Found null pointer for '%s'"), *ObjectName);
        return nullptr;
    }

    return Found;
}

FString USceneStateTracker::GetActorNameByActor(AActor* Actor) const
{
    if (!Actor)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ GetActorNameByActor FAILED: Null actor"));
        return TEXT("Unknown");
    }

    if (!ActorToNameMap.Contains(Actor))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  GetActorNameByActor FAILED: Actor not found in map"));
        return TEXT("Unknown");
    }

    return ActorToNameMap[Actor];
}

bool USceneStateTracker::RemoveSpawnedActorByName(const FString& ObjectName)
{
    if (!SpawnedActorsByName.Contains(ObjectName))
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ RemoveSpawnedActorByName FAILED: '%s' not found"), *ObjectName);
        return false;
    }

    AActor* ActorToRemove = SpawnedActorsByName[ObjectName];

    if (!ActorToRemove || ActorToRemove->IsActorBeingDestroyed())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ RemoveSpawnedActorByName FAILED: Invalid actor for '%s'"), *ObjectName);
        SpawnedActorsByName.Remove(ObjectName);
        return false;
    }

    SpawnedActorsByName.Remove(ObjectName);
    ActorToNameMap.Remove(ActorToRemove);

    int32 Index = SpawnedActors.Find(ActorToRemove);
    if (Index != INDEX_NONE)
    {
        SpawnedActors.RemoveAt(Index);
    }

    ActorToRemove->Destroy();

    UE_LOG(LogTemp, Warning, TEXT("🗑️  Removed: %s | Remaining: %d"), *ObjectName, SpawnedActors.Num());
    return true;
}

bool USceneStateTracker::RemoveSpawnedActorByActor(AActor* Actor)
{
    if (!Actor)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ RemoveSpawnedActorByActor FAILED: Null actor"));
        return false;
    }

    if (!ActorToNameMap.Contains(Actor))
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ RemoveSpawnedActorByActor FAILED: Actor not in registry"));
        return false;
    }

    FString ObjectName = ActorToNameMap[Actor];
    return RemoveSpawnedActorByName(ObjectName);
}

void USceneStateTracker::RemoveMultipleActors(const TArray<FString>& ObjectNames)
{
    if (ObjectNames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  RemoveMultipleActors: Empty array"));
        return;
    }

    int32 Success = 0, Failed = 0;

    for (const FString& Name : ObjectNames)
    {
        if (RemoveSpawnedActorByName(Name))
        {
            Success++;
        }
        else
        {
            Failed++;
        }
    }

    if (Failed > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  RemoveMultipleActors: %d removed, %d failed"), Success, Failed);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("✅ RemoveMultipleActors: %d removed"), Success);
    }
}

void USceneStateTracker::ClearAllSpawnedActors()
{
    int32 Destroyed = 0;
    int32 AlreadyDestroying = 0;  // ✅ CHANGE 1: Track actors already queued for destruction

    for (AActor* Actor : SpawnedActors)
    {
        if (!IsValid(Actor))  // ✅ CHANGE 2: Use IsValid() instead of raw null check
        {
            AlreadyDestroying++;
            continue;
        }
        
        if (!Actor->IsActorBeingDestroyed())
        {
            Actor->Destroy();
            Destroyed++;
        }
        else
        {
            AlreadyDestroying++;  // ✅ CHANGE 3: Track actors in destruction queue
        }
    }

    SpawnedActors.Reset();
    SpawnedActorsByName.Reset();
    ActorToNameMap.Reset();

    // ✅ CHANGE 4: CRITICAL - Reset location occupancy in LocationQueryEngine
    if (LocationEngine)
    {
        LocationEngine->ClearAllOccupancy();
    }

    // ✅ CHANGE 5: Enhanced logging with destruction status
    UE_LOG(LogTemp, Warning, TEXT("🗑️  ClearAllSpawnedActors: Destroyed %d, Already destroying %d"), 
        Destroyed, AlreadyDestroying);
}

void USceneStateTracker::LogSceneStateVerbose()
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("📊 Scene State:"));
    UE_LOG(LogTemp, Display, TEXT("   AssetIndexer: %s"), AssetIndexer ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Display, TEXT("   LocationEngine: %s"), LocationEngine ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Display, TEXT("   GenAISystem: %s"), GenAISystem ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Display, TEXT("   SceneBuilder: %s"), SceneBuilder ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Display, TEXT("   HistoryManager: %s"), HistoryManager ? TEXT("✅") : TEXT("❌"));
    UE_LOG(LogTemp, Display, TEXT("   Spawned: %d actors"), SpawnedActors.Num());
    UE_LOG(LogTemp, Display, TEXT(""));
}

void USceneStateTracker::DelayedInit()
{
    if (LocationEngine)
    {
        // NOW we scan. The actors should be loaded by now.
        LocationEngine->InitializePlayableAreaBounds();
        LocationEngine->ScanWorldLocationsAsync(GetWorld());
    }
}

void USceneStateTracker::VisualizeBounds(float Duration)
{
    if (LocationEngine)
    {
        LocationEngine->VisualizePlayableAreaBounds(Duration);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ LocationEngine not initialized!"));
    }
}


void USceneStateTracker::ClearAllGeneratedContent()
{
    UE_LOG(LogTemp, Warning, TEXT("☢️ SceneStateTracker: INITIATING GLOBAL CONTENT CLEAR ☢️"));

    // 1. Destroy all temporary props (Handles all particles ATTACHED to spawned actors)
    ClearAllSpawnedActors(); // Assuming this is the existing function you mentioned

    // 2. Strip effects from persistent actors (Handles all particles ATTACHED to Player/Level)
    ClearParticlesFromPersistentActors();

    // Add any future environment/light resets here if needed
}


void USceneStateTracker::ClearParticlesFromPersistentActors()
{
    int32 TotalEffectsStripped = 0;
    
    // Iterate through all actors we've marked as persistent
    for (AActor* TargetActor : ActorsToCleanEffectsFrom)
    {
        if (!TargetActor || !IsValid(TargetActor)) continue;

        // Get all components of the actor
        TArray<UActorComponent*> Components;
        TargetActor->GetComponents(Components);

        for (UActorComponent* Comp : Components)
        {
            // Check for the unique tag used by the SceneBuilder for generated FX
            if (Comp && Comp->ComponentTags.Contains(TEXT("GenAI_FX")))
            {
                // Double-check it's a particle system (optional, but safer)
                if (Comp->IsA<UNiagaraComponent>()) 
                {
                    Comp->DestroyComponent(); // Destroys the component, not the Actor
                    TotalEffectsStripped++;
                }
            }
            
        }
        TargetActor->Tags.Remove(FName("HasGenAI_FX"));
    }

    UE_LOG(LogTemp, Log, TEXT("   ✨ Stripped %d particle effects from persistent actors."), TotalEffectsStripped);
}

void USceneStateTracker::RegisterActorForEffectCleanup(AActor* Actor)
{
    if (Actor && !ActorsToCleanEffectsFrom.Contains(Actor))
    {
        ActorsToCleanEffectsFrom.Add(Actor);
        UE_LOG(LogTemp, Log, TEXT("   📝 Registered persistent actor for cleanup: %s"), *Actor->GetName());
    }
}