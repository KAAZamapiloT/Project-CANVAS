// Fill out your copyright notice in the Description page of Project Settings.

#include "SceneStateTracker.h"
#include "GenAISystem.h"
#include "SceneBuilder.h"
#include "SceneHistoryManager.h"
#include"ScenePlan.h"
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
            LocationEngine->ScanWorldLocationsAsync(GetWorld());
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

    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("╔═══════════════════════════════════════════╗"));
    UE_LOG(LogTemp, Warning, TEXT("║  📥 Plan Received - %d props, %d spawn    ║"), 
        Plan.Props.Num(), Plan.SpawnRequest.Num());
    UE_LOG(LogTemp, Warning, TEXT("╚═══════════════════════════════════════════╝"));

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

void USceneStateTracker::ResolveLocationsInPlan(FEnhancedScenePlan& Plan)
{
    if (!LocationEngine)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ ResolveLocationsInPlan FAILED: LocationEngine is null"));
        return;
    }

    int32 Resolved = 0, Failed = 0;

    for (FSpawnRequest& Request : Plan.SpawnRequest)
    {
        if (Request.LocationName.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("  ⚠️  Empty LocationName for '%s'"), *Request.ObjectName);
            Failed++;
            continue;
        }

        FSpawnLocation ResolvedLoc = LocationEngine->ResolveLocation(Request.LocationName);
        FVector BaseLocation = ResolvedLoc.WorldPosition;

        if (!LocationEngine->IsLocationClear(BaseLocation, Request.ClearanceRadius))
        {
            FSpawnLocation Alternative = LocationEngine->FindNearestFreeLocation(
                BaseLocation,
                Request.ClearanceRadius
            );
            BaseLocation = Alternative.WorldPosition;
        }

        FVector FinalLocation = BaseLocation + Request.LocationOffset;
        Request.SpawnLocation = FinalLocation;

        LocationEngine->SetLocationOccupied(Request.LocationName, true, nullptr);
        Resolved++;
    }

    if (Failed > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("  ⚠️  ResolveLocationsInPlan: %d/%d resolved"), 
            Resolved, Resolved + Failed);
    }
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

    for (AActor* Actor : SpawnedActors)
    {
        if (Actor && !Actor->IsActorBeingDestroyed())
        {
            Actor->Destroy();
            Destroyed++;
        }
    }

    SpawnedActors.Reset();
    SpawnedActorsByName.Reset();
    ActorToNameMap.Reset();

    UE_LOG(LogTemp, Warning, TEXT("🗑️  ClearAllSpawnedActors: Destroyed %d actors"), Destroyed);
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

