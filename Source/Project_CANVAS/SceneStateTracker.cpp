// Fill out your copyright notice in the Description page of Project Settings.

#include "SceneStateTracker.h"
#include "GenAISystem.h"
#include "SceneBuilder.h"
#include "SceneHistoryManager.h"
#include"ScenePlan.h"
#include"LocationQueryEngine.h"
#include "API_KEY.h"
void USceneStateTracker::Init()
{
    Super::Init();

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
            API_KEY a;
            LocationEngine->ScanWorldLocationsAsync(GetWorld());
            LocationEngine->ConfigureLLMFallback(
           TEXT("https://api.groq.com/openai/v1/chat/completions"),
           a.GetGroqKey(),
           TEXT("openai/gpt-oss-20b")
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
}

void USceneStateTracker::OnStart()
{
    Super::OnStart();
    
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("🚀 SceneStateTracker::OnStart"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    
    // ✅ World is now GUARANTEED to be available
    UWorld* World = GetWorld();
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
    
    UE_LOG(LogTemp, Display, TEXT("✅ OnStart complete - system fully initialized"));
    World->GetTimerManager().SetTimer(
        InitTimerHandle, 
        this, 
        &USceneStateTracker::DelayedInit, 
        1.0f, // 1 Second Delay
        false
    );
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
  ClearAllSpawnedActors();
  UE_LOG(LogTemp, Warning, TEXT("<Clearing ALL ACTORS>"));
    FEnhancedScenePlan EnrichedPlan = Plan;

    UE_LOG(LogTemp, Display, TEXT("🔄 [1/3] ResolveTexturesFromNames..."));
    ResolveTexturesFromNames(EnrichedPlan);
    UE_LOG(LogTemp, Display, TEXT("✅ Texture resolution done"));

    UE_LOG(LogTemp, Display, TEXT("🔄 [2/3] ResolveMeshesFromNames..."));
    ResolveMeshesFromNames(EnrichedPlan);
    UE_LOG(LogTemp, Display, TEXT("✅ Mesh resolution done"));

    if (EnrichedPlan.bSpawnActors && EnrichedPlan.SpawnRequest.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("🔄 [3/3] ResolveLocationsInPlan..."));
        ResolveLocationsInPlan(EnrichedPlan);
        UE_LOG(LogTemp, Display, TEXT("✅ Location resolution done"));
    }

    if (HistoryManager)
    {
        HistoryManager->SavePlan(EnrichedPlan, UserPrompt);
    }

    if (SceneBuilder)
    {
        UE_LOG(LogTemp, Display, TEXT("🔄 Building scene..."));
        SceneBuilder->BuildScene(EnrichedPlan, GetWorld());
    }

    LogSceneStateVerbose();

    UE_LOG(LogTemp, Warning, TEXT("✅ Plan execution complete"));
    UE_LOG(LogTemp, Warning, TEXT(""));
}

void USceneStateTracker::ResolveTexturesFromNames(FEnhancedScenePlan& Plan)
{
    if (!AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ResolveTexturesFromNames FAILED: AssetIndexer is null"));
        return;
    }

    int32 Resolved = 0, Failed = 0;

    for (FPropsModification& Prop : Plan.Props)
    {
        FString TextureKey = Prop.Texture.BaseColorPath;
        if (TextureKey.IsEmpty()) continue;

        FTextureSet ResolvedSet = AssetIndexer->ResolveTextureFromName(TextureKey);

        if (ResolvedSet.BaseColorPath.IsEmpty())
        {
            ResolvedSet = AssetIndexer->ResolveBaseMaterialToTextureSet(TextureKey);
        }

        if (!ResolvedSet.BaseColorPath.IsEmpty())
        {
            Prop.Texture = ResolvedSet;
            Resolved++;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  ⚠️  Texture '%s' not found"), *TextureKey);
            Failed++;
        }
    }

    if (Failed > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("  ⚠️  ResolveTexturesFromNames: %d/%d resolved"), 
            Resolved, Resolved + Failed);
    }
}

void USceneStateTracker::ResolveMeshesFromNames(FEnhancedScenePlan& Plan)
{
    if (!AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ResolveMeshesFromNames FAILED: AssetIndexer is null"));
        return;
    }

    int32 Resolved = 0, Failed = 0;

    for (FSpawnRequest& Spawn : Plan.SpawnRequest)
    {
        if (Spawn.AssetPath.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("  ⚠️  Empty AssetPath for '%s'"), *Spawn.ObjectName);
            Failed++;
            continue;
        }

        FString ResolvedMesh = AssetIndexer->ResolveMeshToFullPath(Spawn.AssetPath);
        if (!ResolvedMesh.IsEmpty())
        {
            Spawn.AssetPath = ResolvedMesh;
            Resolved++;
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  ⚠️  Mesh '%s' not found"), *Spawn.AssetPath);
            Failed++;
        }
    }

    if (Failed > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("  ⚠️  ResolveMeshesFromNames: %d/%d resolved"), 
            Resolved, Resolved + Failed);
    }
}

// SceneStateTracker.cpp

void USceneStateTracker::ResolveLocationsInPlan(FEnhancedScenePlan& Plan)
{
    if (!LocationEngine || !LocationEngine->GetLLMResolver())
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ResolveLocationsInPlan FAILED: Components missing"));
        return;
    }

    // 1. Store Plan Globally (So we can access it when the Async callback fires)
    PendingPlan = Plan; 

    // 2. Identify which items need LLM help
    TArray<FSpawnRequest> BatchRequests;
    
    // Track occupied spots from local resolution
    TSet<FVector> LocalOccupiedPositions;

    UE_LOG(LogTemp, Display, TEXT("🔄 Phase 1: Separating Local vs Remote requests..."));

    for (int32 i = 0; i < PendingPlan.SpawnRequest.Num(); i++)
    {
        FSpawnRequest& Req = PendingPlan.SpawnRequest[i];

        // --- CHECK: Can we resolve this locally? ---
        // We check if it's "CUSTOM:...", "PLAYER_...", or a specific named location in DB
        bool bIsLocal = Req.LocationName.StartsWith("CUSTOM") || 
                        Req.LocationName.Contains("PLAYER") ||
                        Req.LocationName.Contains("CLOSEST") ||
                        Req.LocationName.Contains("NEAR");

        if (bIsLocal)
        {
            // Resolve immediately
            FVector LocalPos = LocationEngine->ResolveLocationName(Req.LocationName);
            
            // Validate
            if (!LocalPos.IsZero() && LocationEngine->IsLocationClear(LocalPos, Req.ClearanceRadius))
            {
                Req.SpawnLocation = LocalPos;
                LocalOccupiedPositions.Add(LocalPos);
                LocationEngine->SetLocationOccupied(Req.LocationName, true);
                UE_LOG(LogTemp, Display, TEXT("   ✅ Local Resolve: %s -> %s"), *Req.ObjectName, *LocalPos.ToString());
            }
            else
            {
                // Local failed? Add to AI batch as fallback
                UE_LOG(LogTemp, Warning, TEXT("   ⚠️ Local blocked/failed for '%s'. Queueing for AI."), *Req.ObjectName);
                BatchRequests.Add(Req);
            }
        }
        else
        {
            // It is a semantic zone (BACKGROUND, LEFT_SIDE, etc) -> Queue for AI
            BatchRequests.Add(Req);
        }
    }

    // --- THE FORK IN THE ROAD ---

    if (BatchRequests.Num() == 0)
    {
        // BRANCH A: Everything resolved locally! Build now.
        UE_LOG(LogTemp, Display, TEXT("🚀 All locations resolved locally. Building immediately."));
        FinalizeSceneBuild(TMap<FString, FResolutionResult>()); 
    }
    else
    {
        // BRANCH B: We have items for the LLM.
        UE_LOG(LogTemp, Warning, TEXT("⏳ Sending Async Batch for %d items... Game continues."), BatchRequests.Num());
        
        // CALL ASYNC - Note we bind 'FinalizeSceneBuild' as the callback
        LocationEngine->GetLLMResolver()->ResolveBatchLocationsAsync(
            BatchRequests,
            LocationEngine->GetLocationContextForLLM(),
            FOnBatchLocationsResolved::CreateUObject(this, &USceneStateTracker::FinalizeSceneBuild)
        );
        
        // 🛑 EXIT FUNCTION. DO NOT BUILD YET.
    }
}

void USceneStateTracker::FinalizeSceneBuild(const TMap<FString, FResolutionResult>& AsyncResults)
{
    UE_LOG(LogTemp, Display, TEXT("🔄 Phase 2: Finalizing Scene Plan..."));

    // --- CHECK FOR API FAILURE (HTTP 429 / Timeout) ---
    bool bApiFailed = (AsyncResults.Num() == 0);
    if (bApiFailed)
    {
        UE_LOG(LogTemp, Error, TEXT("⚠️ Async Results Empty (API Failure or Rate Limit). Running Emergency Fallback for ALL items."));
    }

    // 1. APPLY LLM RESULTS (If available)
    if (!bApiFailed)
    {
        for (FSpawnRequest& Req : PendingPlan.SpawnRequest)
        {
            if (AsyncResults.Contains(Req.ObjectName))
            {
                const FResolutionResult& Res = AsyncResults[Req.ObjectName];
                
                // Apply Basic Transform
                Req.SpawnLocation = Res.Location;
                Req.Rotation = FRotator(0, Res.RotationYaw, 0);
                
                // --- SMART SCALE LOGIC ---
                // Determine if we should apply the LLM's scale or clamp it
                float TargetScale = Res.Scale;
                
                // Load the mesh to check its physical size
                if (!Req.AssetPath.IsEmpty())
                {
                    UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Req.AssetPath));
                    if (Mesh)
                    {
                        FBoxSphereBounds Bounds = Mesh->GetBounds();
                        float MaxSize = Bounds.BoxExtent.GetMax() * 2.0f; // Diameter in cm
                        
                        // If object is HUGE (>20 meters) and LLM wants to scale it UP
                        if (MaxSize > 2000.0f && TargetScale > 1.0f)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("📉 Clamping giant mesh '%s' (Size: %.0f). Scale %.2f -> 1.0"), 
                                *Req.ObjectName, MaxSize, TargetScale);
                            TargetScale = 1.0f;
                        }
                    }
                }
                
                Req.Scale = FVector(TargetScale);
                
                UE_LOG(LogTemp, Display, TEXT("   🧠 AI Placed: %s -> %s (Scale: %.1f)"), 
                    *Req.ObjectName, *Req.SpawnLocation.ToString(), TargetScale);
            }
        }
    }

    // 2. EMERGENCY FALLBACK / VALIDATION
    // Runs for ANY item that is still at (0,0,0) OR if the API failed entirely
    for (FSpawnRequest& Req : PendingPlan.SpawnRequest)
    {
        // Treat (0,0,0) as an error state for generated content
        if (Req.SpawnLocation.IsNearlyZero())
        {
            UE_LOG(LogTemp, Warning, TEXT("   ⚠️ Generating Fallback for '%s'"), *Req.ObjectName);
            
            // Use Local Algorithm to find a safe spot
            FSpawnLocation FallbackLoc = LocationEngine->FindValidSpawnLocation(
                Req.LocationName, 
                Req.ClearanceRadius
            );
            
            Req.SpawnLocation = FallbackLoc.WorldPosition;
            
            // Add slight jitter to prevent perfect stacking if multiple fail
            Req.SpawnLocation.X += FMath::RandRange(-50.0f, 50.0f);
            Req.SpawnLocation.Y += FMath::RandRange(-50.0f, 50.0f);
        }
        
        // CRITICAL: Mark as occupied so physics/logic knows this spot is taken
        LocationEngine->SetLocationOccupied(Req.LocationName, true);
    }

    // 3. EXECUTE BUILD (Guaranteed Single Execution)
    if (SceneBuilder)
    {
        UE_LOG(LogTemp, Warning, TEXT("🎬 Triggering SceneBuilder with %d actors"), PendingPlan.SpawnRequest.Num());
        SceneBuilder->BuildScene(PendingPlan, GetWorld());
    }
    
    // 4. Save History
    if (HistoryManager)
    {
        HistoryManager->SavePlan(PendingPlan, "Finalized Build");
    }
    
    // Cleanup
    PendingPlan = FEnhancedScenePlan();
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
