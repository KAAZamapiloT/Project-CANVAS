// Fill out your copyright notice in the Description page of Project Settings.


// AssetIndexer.cpp
#include "AssetIndexer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "Particles/ParticleSystem.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include"ScenePlan.h"
#include "Engine/StaticMesh.h"
#include "AssetIndexer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include"NiagaraSystem.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
void UAssetIndexer::ScanAllAssetsAsync(UWorld* WorldContext)
{
    if (bIsScanning)
    {
        UE_LOG(LogTemp, Warning, TEXT("AssetIndexer: Scan already in progress."));
        return;
    }
    ScanStartTime=FPlatformTime::Seconds();
    bIsScanning = true;
    bIsScanComplete = false;
    PendingScans = 0;
    
    // Clear old data
    DiscoveredTextureNames.Empty();
    DiscoveredParticleNames.Empty();
    DiscoveredPostProcessNames.Empty();
    DiscoveredActorTags.Empty();
    DiscoveredStaticMeshNames.Empty();
    DiscoveredMeshes.Empty();        // ← ADD THIS
    VariantGroups.Empty();           // ← ADD THIS
   
    
    UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Starting comprehensive asset scan..."));

    // Launch all scans
    PendingScans = 5; // Textures, Particles, PostProcess, ActorTags
    
    ScanForTexturesAsync(TEXT("/Game/DATABASE/textures"));
    ScanForParticlesAsync(TEXT("/Game/DATABASE/particles"));
    ScanForPostProcessMaterialsAsync(TEXT("/Game/DATABASE/postprocess"));
    ScanForStaticMeshesAsync(TEXT("/Game/DATABASE/meshes"));
    ScanActorTagsInLevel(WorldContext);

    if (WorldContext)
    {
        FTimerHandle TimeoutHandle;
        FTimerDelegate TimeoutDelegate;
        
        TimeoutDelegate.BindLambda([this]()
        {
            double Elapsed = FPlatformTime::Seconds() - ScanStartTime;
            if (bIsScanning && Elapsed > 20.0)
            {
                UE_LOG(LogTemp, Error, TEXT("⚠️ Scan timeout (%.2fs), forcing completion"), Elapsed);
                bIsScanning = false;
                bIsScanComplete = true;
                PendingScans = 0;
                MaterialDatabase = BuildMaterialDatabase();
                OnScanComplete.Broadcast();
            }
        });
        
        WorldContext->GetTimerManager().SetTimer(TimeoutHandle, TimeoutDelegate, 20.0f, false);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ WorldContext is null, no timeout protection!"));
    }

}

void UAssetIndexer::ScanForTexturesAsync(FString ScanPath)
{
    // [PHASE 1: GAME THREAD] Query Registry (Must be done here)
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(*ScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);

    int32 AssetCount = AssetList.Num();

    // [PHASE 2: BACKGROUND THREAD] Process Data (Heavy String Ops)
    Async(EAsyncExecution::Thread, [this, ScanPath, AssetList = MoveTemp(AssetList), AssetCount]()
    {
        TArray<FString> LocalPaths;
        LocalPaths.Reserve(AssetCount);

        for (const FAssetData& AssetData : AssetList)
        {
            LocalPaths.Add(AssetData.GetSoftObjectPath().ToString());
        }

        // [PHASE 3: GAME THREAD] Commit Results (Safe)
        AsyncTask(ENamedThreads::GameThread, [this, LocalPaths = MoveTemp(LocalPaths), AssetCount]()
        {
            DiscoveredTextureNames.Append(LocalPaths);
            UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d textures (Async)"), AssetCount);
            CheckAllScansComplete();
        });
    });
}
// AssetIndexer.cpp

void UAssetIndexer::ScanForParticlesAsync(FString ScanPath)
{
    // [PHASE 1: GAME THREAD]
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(*ScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);
    int32 AssetCount = AssetList.Num();

    // [PHASE 2: BACKGROUND THREAD]
    Async(EAsyncExecution::Thread, [this, AssetList = MoveTemp(AssetList), AssetCount]()
    {
        TArray<FString> LocalPaths;
        LocalPaths.Reserve(AssetCount);

        for (const FAssetData& AssetData : AssetList)
        {
            LocalPaths.Add(AssetData.GetSoftObjectPath().ToString());
        }

        // [PHASE 3: GAME THREAD]
        AsyncTask(ENamedThreads::GameThread, [this, LocalPaths = MoveTemp(LocalPaths), AssetCount]()
        {
            DiscoveredParticleNames = LocalPaths;
            UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d Niagara systems (Async)"), AssetCount);
            CheckAllScansComplete();
        });
    });
}

// AssetIndexer.cpp

void UAssetIndexer::ScanForPostProcessMaterialsAsync(FString ScanPath)
{
    // [PHASE 1: GAME THREAD]
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    
    FARFilter Filter;
    Filter.ClassPaths.Add(UMaterialInterface::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(*ScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);
    int32 AssetCount = AssetList.Num();

    // [PHASE 2: BACKGROUND THREAD]
    Async(EAsyncExecution::Thread, [this, AssetList = MoveTemp(AssetList), AssetCount]()
    {
        TArray<FString> LocalPaths;
        LocalPaths.Reserve(AssetCount);

        for (const FAssetData& AssetData : AssetList)
        {
            LocalPaths.Add(AssetData.GetSoftObjectPath().ToString());
        }

        // [PHASE 3: GAME THREAD]
        AsyncTask(ENamedThreads::GameThread, [this, LocalPaths = MoveTemp(LocalPaths), AssetCount]()
        {
            DiscoveredPostProcessNames = LocalPaths;
            UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d PP Materials (Async)"), AssetCount);
            CheckAllScansComplete();
        });
    });
}
void UAssetIndexer::ScanActorTagsInLevel(UWorld* WorldContext)
{
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Error, TEXT("AssetIndexer: WorldContext is null!"));
        CheckAllScansComplete();
        return;
    }
TWeakObjectPtr<UWorld> WeakWorld(WorldContext);
    
    AsyncTask(ENamedThreads::GameThread, [this, WeakWorld]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning actor tags in current level..."));
        
        TArray<AActor*> AllActors;
        UWorld* WorldContext = WeakWorld.Get();
        if (!WorldContext||!IsValid(WorldContext))
        {
            return;
        }
        UGameplayStatics::GetAllActorsOfClass(WorldContext, AActor::StaticClass(), AllActors);
        
        TSet<FString> UniqueTagsSet; // Use set to avoid duplicates
        UniqueTagsSet.Add(TEXT("Player.Character"));
        for (AActor* Actor : AllActors)
        {
            if (Actor)
            {
                for (const FName& Tag : Actor->Tags)
                {
                    // Only add tags that follow your naming convention
                    FString TagString = Tag.ToString();
                    if (!TagString.IsEmpty()) 
                    {
                        UniqueTagsSet.Add(TagString);
                    }
                }
            }
        }
        
        // Convert set to array
        DiscoveredActorTags = UniqueTagsSet.Array();
        
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d unique actor tags"), DiscoveredActorTags.Num());
        CheckAllScansComplete();
    });
}

// In AssetIndexer.cpp

void UAssetIndexer::ScanAssetsOfType(const UClass* AssetClass, FString ScanPath, TArray<FString>& OutArray)
{
    if (!AssetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("AssetIndexer: AssetClass is null!"));
        return;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    
    FARFilter Filter;
    Filter.ClassPaths.Add(AssetClass->GetClassPathName());
    Filter.PackagePaths.Add(FName(*ScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetDataArray;
    AssetRegistryModule.Get().GetAssets(Filter, AssetDataArray);

    OutArray.Empty();
    for (const FAssetData& AssetData : AssetDataArray)
    {
        // ✅ CRITICAL FIX: Store the Full Soft Object Path string
        // The Async Loader needs the full path (e.g. "/Game/Textures/T_Wood.T_Wood")
        // The old code used AssetName.ToString() which is just "T_Wood" -> Fails loading.
        OutArray.Add(AssetData.GetSoftObjectPath().ToString());
    }

    UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d assets of type %s"), 
           OutArray.Num(), *AssetClass->GetName());
}
void UAssetIndexer::CheckAllScansComplete()
{
    FPlatformAtomics::InterlockedDecrement(&PendingScans);
;
    
    if (PendingScans <= 0)
    {
        bIsScanning = false;
        bIsScanComplete = true;
        
        MaterialDatabase = BuildMaterialDatabase();
        
        // ========================================
        // SUMMARY LOG
        // ========================================
        UE_LOG(LogTemp, Warning, TEXT(""));
        UE_LOG(LogTemp, Warning, TEXT("╔════════════════════════════════════════╗"));
        UE_LOG(LogTemp, Warning, TEXT("║  ✅ SCAN COMPLETE                       ║"));
        UE_LOG(LogTemp, Warning, TEXT("╚════════════════════════════════════════╝"));
        UE_LOG(LogTemp, Warning, TEXT("  📷 Textures: %d"), DiscoveredTextureNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  🎆 Particles: %d"), DiscoveredParticleNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  🎨 PostProcess: %d"), DiscoveredPostProcessNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  📦 StaticMeshes: %d"), DiscoveredStaticMeshNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  🏷️  Actor Tags: %d"), DiscoveredActorTags.Num());
        UE_LOG(LogTemp, Warning, TEXT("════════════════════════════════════════"));
        
        // ========================================
        // DETAILED ACTOR TAGS
        // ========================================
        if (DiscoveredActorTags.Num() > 0)
        {
            UE_LOG(LogTemp, Warning, TEXT(""));
            UE_LOG(LogTemp, Warning, TEXT("╔════════════════════════════════════════╗"));
            UE_LOG(LogTemp, Warning, TEXT("║  📍 Discovered Actor Tags              ║"));
            UE_LOG(LogTemp, Warning, TEXT("╚════════════════════════════════════════╝"));
            
            for (const FString& Tag : DiscoveredActorTags)
            {
                UE_LOG(LogTemp, Display, TEXT("  🏷️  %s"), *Tag);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("  ⚠️  No actor tags found in level"));
        }
        
        UE_LOG(LogTemp, Warning, TEXT("════════════════════════════════════════"));
        UE_LOG(LogTemp, Warning, TEXT(""));
        
        OnScanComplete.Broadcast();
    }
}

FString UAssetIndexer::ResolvePostProcessPath(const FString& SearchName)
{
    if (SearchName.IsEmpty()) return TEXT("");
    FString NormalizedSearch = SearchName.ToLower();

    UE_LOG(LogTemp, Display, TEXT("AssetIndexer: Resolving PP '%s'"), *SearchName);

    // Search DiscoveredPostProcessNames
    for (const FString& FullPath : DiscoveredPostProcessNames)
    {
        FString Filename = FPaths::GetBaseFilename(FullPath).ToLower();
        
        // Exact or Substring match
        if (Filename.Equals(NormalizedSearch) || Filename.Contains(NormalizedSearch))
        {
            UE_LOG(LogTemp, Display, TEXT("   ✅ Match: %s"), *FullPath);
            return FullPath;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("   ❌ PP Material not found: %s"), *SearchName);
    return TEXT("");
}

TMap<FString, FTextureSet> UAssetIndexer::BuildMaterialDatabase()
{
    TMap<FString, FTextureSet> Database;

    UE_LOG(LogTemp, Display, TEXT("AssetIndexer: Indexing %d textures via Smart Tokenizer..."), DiscoveredTextureNames.Num());

    for (const FString& FullPath : DiscoveredTextureNames)
    {
        // 1. Analyze
        FParsedTextureInfo Info = AnalyzeTexturePath(FullPath);
        if (Info.BaseName.IsEmpty()) continue;

        // 2. Init Entry
        if (!Database.Contains(Info.BaseName))
        {
            Database.Add(Info.BaseName, FTextureSet());
        }

        FTextureSet& Set = Database[Info.BaseName];

        // 3. Map Path to Slot
        switch (Info.Type)
        {
            case ETextureMapType::Diffuse:
                Set.BaseColorPath = FullPath;
                break;
            case ETextureMapType::Normal:
                Set.NormalPath = FullPath;
                break;
            case ETextureMapType::Roughness:
                Set.RoughnessPath = FullPath;
                break;
            case ETextureMapType::Metallic:
                Set.MetallicPath = FullPath;
                break;
            case ETextureMapType::AO:
                Set.AOPath = FullPath;
                break;
            // ✅ NEW: Handle Displacement
            case ETextureMapType::Displacement:
                Set.DisplacementPath = FullPath;
                break;
            // ✅ NEW: Handle Opacity
            case ETextureMapType::Opacity:
                Set.OpacityPath = FullPath;
                break;
            case ETextureMapType::Packed:
                Set.RoughnessPath = FullPath; 
                Set.MetallicPath = FullPath;
                Set.AOPath = FullPath;
                break;
            case ETextureMapType::Unknown:
                // Fallback for weird files like "Street_Shaft_00"
                if (Set.BaseColorPath.IsEmpty()) Set.BaseColorPath = FullPath;
                break;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("✅ Smart Indexing Complete. Mapped %d unique materials."), Database.Num());
    return Database;
}
FTextureSet UAssetIndexer::ResolveBaseMaterialToTextureSet(const FString& BaseMaterialName)
{
    if (MaterialDatabase.Contains(BaseMaterialName))
    {
        return MaterialDatabase[BaseMaterialName];
    }
    
    UE_LOG(LogTemp, Warning, TEXT("Material not found: %s"), *BaseMaterialName);
    return FTextureSet();  // Empty set
}

FString UAssetIndexer::ExtractMaterialBaseName(const FString& TextureName)
{
    FString BaseName = TextureName;
    // Remove all ureal specific prefix of fab textures 
    TArray<FString> Prefixes={TEXT("T_")};
    // Remove all known PBR suffixes , -> added FAB specific suffixes also
    const TArray<FString> Suffixes = {
        // Complex packed masks
        TEXT("_AO_R_MT"), TEXT("_AO_R_TM"), TEXT("_MRA1"), TEXT("_MRA"), TEXT("_MRO"), 
        TEXT("_RMA"), TEXT("_ORM"), TEXT("_ARM"),
        
        // PBR Standard 2K/4K
        TEXT("_diff_2k"), TEXT("_diff_4k"), TEXT("_rough_2k"), TEXT("_rough_4k"),
        TEXT("_nor_gl_2k"), TEXT("_nor_gl_4k"), TEXT("_nor_dx_2k"), TEXT("_nor_dx_4k"),
        TEXT("_metal_2k"), TEXT("_metal_4k"), TEXT("_ao_2k"), TEXT("_ao_4k"),
        TEXT("_disp_2k"), TEXT("_disp_4k"), TEXT("_arm_2k"), TEXT("_arm_4k"),
        
        // Simple Names
        TEXT("_BaseColor"), TEXT("_Albedo"), TEXT("_Normal"), TEXT("_Roughness"), 
        TEXT("_Metallic"), TEXT("_Metalness"), TEXT("_Metal"), TEXT("_Mask"), 
        TEXT("_Height"), TEXT("_Occlusion"), TEXT("_Gloss"),
        
        // Short Codes
        TEXT("_BC"), TEXT("_D"), TEXT("_N"), TEXT("_R"), TEXT("_M"), TEXT("_A"), 
        TEXT("_H"), TEXT("_OCC"), TEXT("_nm"), TEXT("_alb"),
        
        // Numbered variations at end
        TEXT("_00"), TEXT("_01"), TEXT("_02") , TEXT("_03"), TEXT("_04"), TEXT("_05"),
        TEXT("_06")
    };
    
    for (const FString& Suffix : Suffixes)
    {
        if (BaseName.EndsWith(Suffix))
        {
            BaseName.RemoveFromEnd(Suffix);
            break;
        }
    }
    for (const FString& Prefix : Prefixes)
    {
        if (BaseName.StartsWith(Prefix))
        {
            BaseName.RemoveFromStart(Prefix);
            break;
        }
    }
    return BaseName;
}

FTextureSet UAssetIndexer::ResolveTextureFromName(const FString& SearchName)
{
    FTextureSet Result;
    if (SearchName.IsEmpty()) return Result;

    // 1. Analyze the Search Query
    // Use AnalyzeTexturePath to strip "T_" prefixes and "Diff/Norm" suffixes
    FParsedTextureInfo SearchInfo = AnalyzeTexturePath(SearchName);
    FString SearchBase = SearchInfo.BaseName.ToLower();
    
    // Extra safety: Normalize spaces to underscores (LLM might say "Wood Floor")
    SearchBase.ReplaceInline(TEXT(" "), TEXT("_"));

    UE_LOG(LogTemp, Display, TEXT("🔍 ResolveTexture: Query='%s' -> Base='%s'"), *SearchName, *SearchBase);

    // ========================================================
    // STRATEGY 1: EXACT MATCH (Highest Priority)
    // ========================================================
    // Check if we have a direct key in the map (faster than loop)
    // We loop here because map keys might be Case Sensitive depending on creation
    for (const auto& Pair : MaterialDatabase)
    {
        if (Pair.Key.ToLower().Equals(SearchBase))
        {
            UE_LOG(LogTemp, Display, TEXT("   ✅ [EXACT] Found Material Set: %s"), *Pair.Key);
            return Pair.Value;
        }
    }

    // ========================================================
    // STRATEGY 2: SUBSTRING MATCH
    // ========================================================
    // "Wood" should match "Old_Wood_Floor"
    for (const auto& Pair : MaterialDatabase)
    {
        FString DbKey = Pair.Key.ToLower();
        
        if (DbKey.Contains(SearchBase) || SearchBase.Contains(DbKey))
        {
            UE_LOG(LogTemp, Display, TEXT("   ⚠️ [SUBSTRING] '%s' matched '%s'"), *SearchName, *Pair.Key);
            return Pair.Value;
        }
    }

    // ========================================================
    // STRATEGY 3: FUZZY SIMILARITY (Robust Fallback)
    // ========================================================
    // Uses CalculateSimilarity to catch typos ("WoodPlank" vs "WoodenPlank")
    FString BestMatchKey;
    int32 BestScore = 0;

    for (const auto& Pair : MaterialDatabase)
    {
        FString DbKey = Pair.Key.ToLower();
        
        // Use your existing helper function
        int32 Score = CalculateSimilarity(SearchBase, DbKey);
        
        // Threshold: 50% similarity (slightly looser for textures than meshes)
        if (Score > BestScore && Score > 50) 
        {
            BestScore = Score;
            BestMatchKey = Pair.Key;
        }
    }

    if (!BestMatchKey.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ [FUZZY %d%%] '%s' matched '%s'"), BestScore, *SearchName, *BestMatchKey);
        return MaterialDatabase[BestMatchKey];
    }

    UE_LOG(LogTemp, Warning, TEXT("❌ Texture set not found for: %s"), *SearchName);
    return Result;
}

void UAssetIndexer::ScanForStaticMeshesAsync(FString ScanPath)
{
    // [PHASE 1: GAME THREAD] Query Registry
    UE_LOG(LogTemp, Display, TEXT("🔍 Scanning static meshes in: %s"), *ScanPath);
    
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    FARFilter Filter;
    Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(*ScanPath));
    Filter.bRecursivePaths = true;
    
    TArray<FAssetData> AssetList;
    AssetRegistryModule.Get().GetAssets(Filter, AssetList);
    int32 AssetCount = AssetList.Num();

    UE_LOG(LogTemp, Display, TEXT("   Found %d static meshes"), AssetCount);

    // [PHASE 2: BACKGROUND THREAD] Heavy Processing & Tokenizing
    Async(EAsyncExecution::Thread, [this, ScanPath, AssetList = MoveTemp(AssetList)]()
    {
        // Thread-Local Storage to avoid locking
        TMap<FString, FMeshAssetInfo> LocalMeshes;
        TMap<FString, FMeshVariantGroup> LocalGroups;
        TArray<FString> LocalNames;

        for (const FAssetData& AssetData : AssetList)
        {
            FString MeshName = AssetData.AssetName.ToString();
            FString FullPath = AssetData.GetSoftObjectPath().ToString();
            
            // Validation
            if (FullPath.Contains(TEXT("//"))) continue; 
            FString Directory = FPaths::GetPath(FullPath);
            
            // --- SMART ANALYSIS ---
            FParsedMeshInfo Analysis = AnalyzeMeshName(MeshName);

            // --- BUILD STRUCT ---
            FMeshAssetInfo MeshInfo;
            MeshInfo.MeshName = MeshName;
            MeshInfo.MeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FullPath));
            MeshInfo.FullPath = FullPath;
            MeshInfo.Directory = Directory;
            MeshInfo.Keywords = Analysis.Keywords;
            
            // --- INDEXING ---
            FString UniqueKey = Analysis.UniqueName.ToLower(); 
            if (!LocalMeshes.Contains(UniqueKey))
            {
                LocalMeshes.Add(UniqueKey, MeshInfo);
            }
            LocalNames.AddUnique(MeshName);
            
            // --- GROUPING ---
            FString BaseKey = Analysis.CleanName.ToLower();
            
            if (!LocalGroups.Contains(BaseKey))
            {
                FMeshVariantGroup Group;
                Group.BaseName = Analysis.CleanName;
                LocalGroups.Add(BaseKey, Group);
            }
            
            LocalGroups[BaseKey].Variants.AddUnique(MeshName);
            LocalGroups[BaseKey].VariantPaths.AddUnique(FullPath);
        }

        // [PHASE 3: GAME THREAD] Commit to Main Variables
        AsyncTask(ENamedThreads::GameThread, [this, LocalMeshes = MoveTemp(LocalMeshes), LocalGroups = MoveTemp(LocalGroups), LocalNames = MoveTemp(LocalNames)]()
        {
            DiscoveredMeshes.Append(LocalMeshes);
            VariantGroups.Append(LocalGroups);
            DiscoveredStaticMeshNames.Append(LocalNames);
            
            int32 VariantGroupCount = 0;
            for (const auto& Group : VariantGroups)
            {
                if (Group.Value.Variants.Num() > 1) VariantGroupCount++;
            }
            
            UE_LOG(LogTemp, Warning, TEXT("✅ Mesh scan: %d meshes, %d variant groups"), 
                DiscoveredMeshes.Num(), VariantGroupCount);
            
            CheckAllScansComplete();
        });
    });
}

FParsedTextureInfo UAssetIndexer::AnalyzeTexturePath(const FString& FullPath)
{
    FParsedTextureInfo Info;
    Info.OriginalPath = FullPath;
    Info.Type = ETextureMapType::Unknown;

    FString Filename = FPaths::GetBaseFilename(FullPath);
    
    // Normalize: Remove 'T_' prefix if present (Case Insensitive)
    if (Filename.StartsWith(TEXT("T_"), ESearchCase::IgnoreCase))
    {
        Filename.RemoveFromStart(TEXT("T_"));
    }

    FString ProcessedName = Filename.Replace(TEXT("-"), TEXT("_")); 

    TArray<FString> Tokens;
    ProcessedName.ParseIntoArray(Tokens, TEXT("_"), true);

    // Dictionaries (Refined)
    const TMap<FString, ETextureMapType> TypeMap = {
        // Diffuse
        {TEXT("BC"), ETextureMapType::Diffuse}, {TEXT("D"), ETextureMapType::Diffuse}, 
        {TEXT("Diff"), ETextureMapType::Diffuse}, {TEXT("Diffuse"), ETextureMapType::Diffuse},
        {TEXT("Albedo"), ETextureMapType::Diffuse}, {TEXT("BaseColor"), ETextureMapType::Diffuse},
        {TEXT("Color"), ETextureMapType::Diffuse},
        
        // Normal
        {TEXT("N"), ETextureMapType::Normal}, {TEXT("NM"), ETextureMapType::Normal},
        {TEXT("Nor"), ETextureMapType::Normal}, {TEXT("Normal"), ETextureMapType::Normal},
        {TEXT("Nor_Gl"), ETextureMapType::Normal}, {TEXT("Nor_Dx"), ETextureMapType::Normal},
        
        // Roughness
        {TEXT("R"), ETextureMapType::Roughness}, {TEXT("Rough"), ETextureMapType::Roughness},
        {TEXT("Roughness"), ETextureMapType::Roughness},
        
        // Metallic
        {TEXT("M"), ETextureMapType::Metallic}, {TEXT("Met"), ETextureMapType::Metallic},
        {TEXT("Metal"), ETextureMapType::Metallic}, {TEXT("Metallic"), ETextureMapType::Metallic},
        {TEXT("Metalness"), ETextureMapType::Metallic}, {TEXT("MT"), ETextureMapType::Metallic},
        
        // AO
        {TEXT("AO"), ETextureMapType::AO}, {TEXT("Amb"), ETextureMapType::AO},
        {TEXT("Occlusion"), ETextureMapType::AO}, {TEXT("OCC"), ETextureMapType::AO},
        
        // Displacement
        {TEXT("Disp"), ETextureMapType::Displacement}, {TEXT("Displacement"), ETextureMapType::Displacement},
        {TEXT("Height"), ETextureMapType::Displacement}, {TEXT("H"), ETextureMapType::Displacement},

        // Opacity
        {TEXT("Opacity"), ETextureMapType::Opacity}, {TEXT("Alpha"), ETextureMapType::Opacity},
        {TEXT("Mask"), ETextureMapType::Opacity}, {TEXT("Trans"), ETextureMapType::Opacity},
        
        // Packed
        {TEXT("ORM"), ETextureMapType::Packed}, {TEXT("ARM"), ETextureMapType::Packed},
        {TEXT("MRO"), ETextureMapType::Packed}, {TEXT("MRA"), ETextureMapType::Packed},
        {TEXT("RMA"), ETextureMapType::Packed}, {TEXT("AO_R_MT"), ETextureMapType::Packed},
        {TEXT("AO_R_TM"), ETextureMapType::Packed}
    };

    TArray<FString> ContentTokens;
    
    for (int32 i = 0; i < Tokens.Num(); i++)
    {
        FString Token = Tokens[i];
        
        // Skip resolution tags
        if (Token.Equals("2k", ESearchCase::IgnoreCase) || 
            Token.Equals("4k", ESearchCase::IgnoreCase) ||
            Token.Equals("8k", ESearchCase::IgnoreCase)) continue;

        // Check Type
        bool bIsType = false;
        for (const auto& Pair : TypeMap)
        {
            if (Token.Equals(Pair.Key, ESearchCase::IgnoreCase))
            {
                Info.Type = Pair.Value;
                bIsType = true;
                break;
            }
        }
        if (bIsType) continue;

        ContentTokens.Add(Token);
    }

    Info.BaseName = FString::Join(ContentTokens, TEXT("_"));
    if (Info.BaseName.IsEmpty()) Info.BaseName = Filename;

    return Info;
}

TSharedPtr<FStreamableHandle> UAssetIndexer::RequestAsyncLoad(
    const TArray<FSoftObjectPath>& AssetsToLoad, 
    FStreamableDelegate DelegateToCall)
{
    // ✅ FIX: Use GetIfInitialized() instead of deprecated GetIfValid()
    if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
    {
        UE_LOG(LogTemp, Display, TEXT("AssetIndexer: Requesting Async Load for %d assets..."), AssetsToLoad.Num());
        
        // This puts the load operation on a background thread
        return Manager->GetStreamableManager().RequestAsyncLoad(
            AssetsToLoad, 
            DelegateToCall, 
            FStreamableManager::AsyncLoadHighPriority
        );
    }
    
    UE_LOG(LogTemp, Error, TEXT("AssetIndexer: AssetManager is invalid! Cannot async load."));
    return nullptr;
}


// AssetIndexer.cpp

FParsedMeshInfo UAssetIndexer::AnalyzeMeshName(const FString& MeshName)
{
    FParsedMeshInfo Info;
    Info.OriginalName = MeshName;
    Info.VariantNumber = -1;

    // 1. Normalize separators (Chair-01 -> Chair_01)
    FString TempName = MeshName;
    TempName.ReplaceInline(TEXT("-"), TEXT("_"));
    TempName.ReplaceInline(TEXT(" "), TEXT("_"));

    // 2. Tokenize
    TArray<FString> Tokens;
    TempName.ParseIntoArray(Tokens, TEXT("_"), true);

    // 3. Define Noise to Ignore
    const TSet<FString> IgnoredTokens = { 
        TEXT("SM"), TEXT("S"), TEXT("T"), TEXT("M"), TEXT("StaticMesh"), 
        TEXT("Mesh"), TEXT("Props"), TEXT("Env"), TEXT("Environment"),
        TEXT("2k"), TEXT("4k"), TEXT("8k"), TEXT("LOD0"), TEXT("LOD1"),
        TEXT("Low"), TEXT("High"), TEXT("Poly")
    };

    TArray<FString> UniqueTokens; // Keeps numbers: "Chair", "01"
    TArray<FString> BaseTokens;   // Removes numbers: "Chair"

    for (int32 i = 0; i < Tokens.Num(); i++)
    {
        FString Token = Tokens[i];
        
        // Skip Prefixes/Noise
        if (IgnoredTokens.Contains(Token)) continue;

        // Check if this token is a Variant Number (e.g. "01", "02", "v1")
        bool bIsNumber = Token.IsNumeric();
        if (!bIsNumber && Token.Len() <= 3 && FChar::IsDigit(Token[Token.Len()-1]))
        {
            // Handles cases like "v1", "A1", "B2"
            bIsNumber = true;
        }

        // ALWAYS add to Unique Name (so we can find specific assets)
        UniqueTokens.Add(Token);
        
        // ONLY add to Base Name if it's NOT a number (This creates the Group)
        if (!bIsNumber)
        {
            BaseTokens.Add(Token);
            // Add to keywords for fuzzy search
            if (Token.Len() > 2) Info.Keywords.AddUnique(Token.ToLower());
        }
        else
        {
            // It's a number, save it
            Info.VariantNumber = FCString::Atoi(*Token);
        }
    }

    // 4. Reconstruct
    // Example Input: "SM_Chair_01"
    Info.UniqueName = FString::Join(UniqueTokens, TEXT("_")); // Result: "Chair_01"
    Info.CleanName = FString::Join(BaseTokens, TEXT("_"));    // Result: "Chair" (THE GROUP NAME)
    
    // Fallback: If name was just "01", keep original
    if (Info.CleanName.IsEmpty()) Info.CleanName = MeshName;
    if (Info.UniqueName.IsEmpty()) Info.UniqueName = MeshName;

    return Info;
}
FString UAssetIndexer::ResolveMeshToFullPathWithVariants(const FString& SearchName)
{
    if (SearchName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("ResolveMesh: Empty search name"));
        return TEXT("");
    }
    
    FString NormalizedSearch = NormalizeName(SearchName);
    
    UE_LOG(LogTemp, Display, TEXT("🎲 ResolveMesh (with variants): '%s'"), *SearchName);
    
    // ========================================
    // STRATEGY 1: Check if requesting specific numbered variant
    // ========================================
    int32 VariantNum = -1;
    if (IsNumberedVariant(SearchName, VariantNum))
    {
        if (DiscoveredMeshes.Contains(NormalizedSearch))
        {
            FString Result = DiscoveredMeshes[NormalizedSearch].FullPath;
            UE_LOG(LogTemp, Display, TEXT("   ✅ [SPECIFIC VARIANT #%d] %s"), VariantNum, *Result);
            return Result;
        }
    }
    
    // ========================================
    // STRATEGY 2: Check variant groups and pick random
    // ========================================
    for (const auto& GroupPair : VariantGroups)
    {
        if (GroupPair.Key.Equals(NormalizedSearch) || 
            GroupPair.Value.BaseName.Contains(SearchName, ESearchCase::IgnoreCase) ||
            SearchName.Contains(GroupPair.Value.BaseName, ESearchCase::IgnoreCase))
        {
            if (GroupPair.Value.VariantPaths.Num() > 0)
            {
                int32 SelectedIdx = FMath::RandRange(0, GroupPair.Value.Variants.Num() - 1);
                FString SelectedVariant = GroupPair.Value.Variants[SelectedIdx];
                FString Result = GroupPair.Value.VariantPaths[SelectedIdx];
                
                UE_LOG(LogTemp, Display, TEXT("   🎲 [VARIANT GROUP] '%s' has %d variants, selected: %s"), 
                    *GroupPair.Value.BaseName, GroupPair.Value.Variants.Num(), *SelectedVariant);
                
                return Result;
            }
        }
    }
    
    // ========================================
    // STRATEGY 3: Fall back to robust single-mesh resolution
    // ========================================
    return ResolveMeshToFullPath(SearchName);
}

FString UAssetIndexer::ResolveMeshToFullPath(const FString& SearchName)
{
    if (SearchName.IsEmpty()) return TEXT("");

    // --- STEP 1: SMART ANALYSIS ---
    // Normalize the input so "SM_Chair_01" becomes Base:"chair", Unique:"chair_01"
    FParsedMeshInfo SearchInfo = AnalyzeMeshName(SearchName);
    
    FString UniqueKey = SearchInfo.UniqueName.ToLower();
    FString BaseKey = SearchInfo.CleanName.ToLower();

    UE_LOG(LogTemp, Display, TEXT("🔎 ResolveMesh: '%s' -> (Base: '%s', Unique: '%s')"), 
        *SearchName, *BaseKey, *UniqueKey);

    // --- STRATEGY 1: EXACT VARIANT MATCH (Highest Priority) ---
    // If the LLM asked for "Chair_01" and we have it, give exactly that.
    if (DiscoveredMeshes.Contains(UniqueKey))
    {
        FString Result = DiscoveredMeshes[UniqueKey].FullPath;
        UE_LOG(LogTemp, Display, TEXT("   ✅ [EXACT] Found: %s"), *Result);
        return Result;
    }

    // --- STRATEGY 2: VARIANT GROUP MATCH (Best for Variety) ---
    // If LLM asked for "Chair", pick a RANDOM valid variant from the group.
    if (VariantGroups.Contains(BaseKey))
    {
        FString Result = VariantGroups[BaseKey].GetRandomVariantPath();
        UE_LOG(LogTemp, Display, TEXT("   🎲 [VARIANT] Group '%s' -> Selected: %s"), *BaseKey, *Result);
        return Result;
    }

    // --- STRATEGY 3: KEYWORD / SUBSTRING SCAN ---
    // Handles cases like "WoodenChair" matching "Chair" or vice versa.
    for (const auto& Pair : DiscoveredMeshes)
    {
        // Check 1: Does the asset name contain our search term?
        if (Pair.Key.Contains(BaseKey))
        {
            UE_LOG(LogTemp, Display, TEXT("   ✅ [SUBSTRING] Found: %s (Matched '%s')"), *Pair.Value.MeshName, *BaseKey);
            return Pair.Value.FullPath;
        }

        // Check 2: Do any keywords match? (e.g. "Wood" in "Chair_Wood")
        for (const FString& SearchKW : SearchInfo.Keywords)
        {
            if (Pair.Value.Keywords.Contains(SearchKW))
            {
                UE_LOG(LogTemp, Display, TEXT("   ✅ [KEYWORD] Found: %s (Matched '%s')"), *Pair.Value.MeshName, *SearchKW);
                return Pair.Value.FullPath;
            }
        }
    }

    // --- STRATEGY 4: FUZZY MATCH (For Typos) ---
    // Catches "Chiar" -> "Chair"
    FString BestMatchPath;
    int32 BestScore = 0;

    for (const auto& Pair : DiscoveredMeshes)
    {
        // Compare the Base Key ("chair") against the asset's cleaned name
        // We use your existing CalculateSimilarity function
        int32 Score = CalculateSimilarity(BaseKey, Pair.Key);
        
        if (Score > BestScore && Score > 60) // Threshold of 60% similarity
        {
            BestScore = Score;
            BestMatchPath = Pair.Value.FullPath;
        }
    }

    if (!BestMatchPath.IsEmpty())
    {
        UE_LOG(LogTemp, Display, TEXT("   ⚠️  [FUZZY %d%%] '%s' -> '%s'"), BestScore, *SearchName, *BestMatchPath);
        return BestMatchPath;
    }

    // --- STRATEGY 5: LAST RESORT (Random Mesh) ---
    // Prevents "invisible" actors if absolutely nothing matches. 
    if (DiscoveredMeshes.Num() > 0)
    {
        TArray<FString> AllKeys;
        DiscoveredMeshes.GetKeys(AllKeys);
        
        // Try to pick something generic (starts with SM_)
        TArray<FString> Candidates;
        for (const FString& Key : AllKeys)
        {
            if (DiscoveredMeshes[Key].MeshName.StartsWith("SM_")) Candidates.Add(Key);
        }

        if (Candidates.Num() > 0)
        {
            FString RandomKey = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
            UE_LOG(LogTemp, Warning, TEXT("   ❌ NO MATCH. Using Random Fallback: %s"), *RandomKey);
            return DiscoveredMeshes[RandomKey].FullPath;
        }
    }

    UE_LOG(LogTemp, Error, TEXT("❌ CRITICAL: No meshes found in database!"));
    return TEXT("");
}


TArray<FString> UAssetIndexer::GetSmartMeshList() const
{
    TArray<FString> SmartList;
    
    // 1. Add all Variant Group Base Names (e.g., "Chair")
    // This covers 90% of your assets if they are well-named
    VariantGroups.GetKeys(SmartList);
    
    // 2. Add "Orphan" meshes (Unique items that didn't form a group)
    // If "Statue_Dragon" exists but has no variants, it might not be in a group depending on your scanner logic.
    // However, your current scanner puts EVERYTHING into a group (even size 1 groups).
    // So iterating VariantGroups.GetKeys is sufficient and safe!
    
    // Optional: Sort for cleaner prompts
    SmartList.Sort();
    
    return SmartList;
}

FMeshVariantGroup UAssetIndexer::GetMeshVariants(const FString& SearchName)
{
    FString NormalizedSearch = NormalizeName(SearchName);
    
    for (const auto& GroupPair : VariantGroups)
    {
        if (GroupPair.Key.Equals(NormalizedSearch) ||
            GroupPair.Value.BaseName.Contains(SearchName, ESearchCase::IgnoreCase))
        {
            return GroupPair.Value;
        }
    }
    
    return FMeshVariantGroup();
}

int32 UAssetIndexer::GetVariantCount(const FString& SearchName)
{
    FMeshVariantGroup Variants = GetMeshVariants(SearchName);
    return Variants.Variants.Num();
}

FMeshAssetInfo UAssetIndexer::GetMeshInfo(const FString& SearchName)
{
    FString NormalizedSearch = NormalizeName(SearchName);
    
    if (DiscoveredMeshes.Contains(NormalizedSearch))
    {
        return DiscoveredMeshes[NormalizedSearch];
    }
    
    return FMeshAssetInfo();
}

FString UAssetIndexer::ExtractBaseName(const FString& VariantName)
{
    FString Result = VariantName;
    
    while (Result.Len() > 0 && FChar::IsDigit(Result[Result.Len() - 1]))
    {
        Result.RemoveAt(Result.Len() - 1);
    }
    
    if (Result.EndsWith(TEXT("_")))
    {
        Result.RemoveAt(Result.Len() - 1);
    }
    
    return Result;
}

bool UAssetIndexer::IsNumberedVariant(const FString& Name, int32& OutVariantNumber)
{
    if (Name.Len() < 3) return false;
    
    int32 LastUnderscore = Name.Find(TEXT("_"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
    if (LastUnderscore == INDEX_NONE) return false;
    
    FString Suffix = Name.RightChop(LastUnderscore + 1);
    
    bool bAllDigits = true;
    for (TCHAR Ch : Suffix)
    {
        if (!FChar::IsDigit(Ch))
        {
            bAllDigits = false;
            break;
        }
    }
    
    if (bAllDigits && Suffix.Len() >= 2)
    {
        OutVariantNumber = FCString::Atoi(*Suffix);
        return true;
    }
    
    return false;
}

TArray<FString> UAssetIndexer::ExtractKeywordsFromMesh(const FString& MeshName)
{
    TArray<FString> Keywords;
    
    FString CleanName = MeshName;
    CleanName.RemoveFromStart(TEXT("SM_"));
    CleanName.RemoveFromStart(TEXT("S_"));
    CleanName.RemoveFromStart(TEXT("T_"));
    CleanName.RemoveFromStart(TEXT("M_"));
    
    // ✅ NEW:
    CleanName.ReplaceInline(TEXT("-"), TEXT("_"));

    TArray<FString> Parts;
    CleanName.ParseIntoArray(Parts, TEXT("_"));
    
    for (FString& Part : Parts)
    {
        if (!Part.IsEmpty() && Part.Len() > 2 && !FChar::IsDigit(Part[0]))
        {
            Keywords.AddUnique(Part.ToLower());
        }
    }
    
    return Keywords;
}

FString UAssetIndexer::NormalizeName(const FString& Name)
{
    FString Normalized = Name.ToLower();
    
    Normalized.RemoveFromStart(TEXT("sm_"));
    Normalized.RemoveFromStart(TEXT("s_"));
    Normalized.RemoveFromStart(TEXT("t_"));
    Normalized.RemoveFromStart(TEXT("m_"));
    
    Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
    Normalized.ReplaceInline(TEXT(" "), TEXT("_"));
    
    Normalized.RemoveFromEnd(TEXT("_2k"));
    Normalized.RemoveFromEnd(TEXT("_4k"));
    Normalized.RemoveFromEnd(TEXT("_8k"));
    Normalized.RemoveFromEnd(TEXT("_hq"));
    Normalized.RemoveFromEnd(TEXT("_lq"));
    
    return Normalized;
}

int32 UAssetIndexer::CalculateSimilarity(const FString& A, const FString& B)
{
    int32 MaxLen = FMath::Max(A.Len(), B.Len());
    if (MaxLen == 0) return 100;
    
    int32 Matches = 0;
    int32 MinLen = FMath::Min(A.Len(), B.Len());
    
    for (int32 i = 0; i < MinLen; i++)
    {
        if (A[i] == B[i])
        {
            Matches++;
        }
    }
    
    return (Matches * 100) / MaxLen;
}

void UAssetIndexer::PrintAllMeshes() const
{
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("╔════════════════════════════════════════╗"));
    UE_LOG(LogTemp, Warning, TEXT("║  📦 Discovered Static Meshes (%d)      ║"), DiscoveredMeshes.Num());
    UE_LOG(LogTemp, Warning, TEXT("╚════════════════════════════════════════╝"));
    
    for (const auto& Pair : DiscoveredMeshes)
    {
        const FMeshAssetInfo& Info = Pair.Value;
        UE_LOG(LogTemp, Display, TEXT("🔹 %s"), *Info.MeshName);
        UE_LOG(LogTemp, Display, TEXT("   Path: %s"), *Info.FullPath);
        UE_LOG(LogTemp, Display, TEXT("   Keywords: %s"), *FString::Join(Info.Keywords, TEXT(", ")));
    }
    UE_LOG(LogTemp, Warning, TEXT(""));
}

void UAssetIndexer::PrintMeshVariants() const
{
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("╔════════════════════════════════════════╗"));
    UE_LOG(LogTemp, Warning, TEXT("║  🎲 Mesh Variant Groups                ║"));
    UE_LOG(LogTemp, Warning, TEXT("╚════════════════════════════════════════╝"));
    
    for (const auto& GroupPair : VariantGroups)
    {
        const FMeshVariantGroup& Group = GroupPair.Value;
        
        if (Group.Variants.Num() > 1)
        {
            UE_LOG(LogTemp, Display, TEXT("📦 %s (%d variants)"), *Group.BaseName, Group.Variants.Num());
            for (int32 i = 0; i < Group.Variants.Num(); i++)
            {
                UE_LOG(LogTemp, Display, TEXT("   [%d] %s"), i + 1, *Group.Variants[i]);
            }
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT(""));
}

TArray<FString> UAssetIndexer::GetAllMeshNames() const
{
    TArray<FString> Names;
    for (const auto& Pair : DiscoveredMeshes)
    {
        Names.Add(Pair.Value.MeshName);
    }
    return Names;
}

TArray<FString> UAssetIndexer::ResolveAllMeshPaths(const FString& SearchName)
{
     TArray<FString> AllMatches;
    
    if (SearchName.IsEmpty())
    {
        return AllMatches;
    }
    
    FString NormalizedSearch = NormalizeName(SearchName);
    
    UE_LOG(LogTemp, Display, TEXT("🔍 ResolveAllMeshPaths: '%s'"), *SearchName);
    
    // === STRATEGY 1: Exact normalized match ===
    if (DiscoveredMeshes.Contains(NormalizedSearch))
    {
        AllMatches.Add(DiscoveredMeshes[NormalizedSearch].FullPath);
        UE_LOG(LogTemp, Display, TEXT("   ✅ [EXACT] Found"));
        return AllMatches;
    }
    
    // === STRATEGY 2: Substring match - GET ALL ===
    for (const auto& Pair : DiscoveredMeshes)
    {
        if (Pair.Value.MeshName.Contains(SearchName, ESearchCase::IgnoreCase) ||
            NormalizedSearch.Contains(Pair.Key))
        {
            AllMatches.Add(Pair.Value.FullPath);
            UE_LOG(LogTemp, Display, TEXT("   ✅ [SUBSTRING] Found: %s"), *Pair.Value.MeshName);
        }
    }
    
    if (AllMatches.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("   📊 Found %d substring matches"), AllMatches.Num());
        return AllMatches;
    }
    
    // === STRATEGY 3: Keyword matching ===
    TArray<FString> SearchKeywords = ExtractKeywordsFromMesh(SearchName);
    
    for (const auto& Pair : DiscoveredMeshes)
    {
        for (const FString& SearchKeyword : SearchKeywords)
        {
            for (const FString& AssetKeyword : Pair.Value.Keywords)
            {
                if (SearchKeyword.Equals(AssetKeyword, ESearchCase::IgnoreCase))
                {
                    AllMatches.AddUnique(Pair.Value.FullPath);
                    UE_LOG(LogTemp, Display, TEXT("   ✅ [KEYWORD] Found: %s"), *Pair.Value.MeshName);
                }
            }
        }
    }
    
    if (AllMatches.Num() > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("   📊 Found %d keyword matches"), AllMatches.Num());
        return AllMatches;
    }
    
    UE_LOG(LogTemp, Error, TEXT("   ❌ No matches found for '%s'"), *SearchName);
    return AllMatches;
}

FString UAssetIndexer::ResolveParticlePath(const FString& SearchName)
{
    if (SearchName.IsEmpty()) return TEXT("");

    FString NormalizedSearch = SearchName.ToLower();

    UE_LOG(LogTemp, Display, TEXT("AssetIndexer: Resolving particle '%s'"), *SearchName);

    // Strategy 1: Exact Match (ignoring case) on the filename
    // e.g. Input: "NS_Fire" -> Matches "/Game/VFX/NS_Fire.NS_Fire"
    for (const FString& FullPath : DiscoveredParticleNames)
    {
        FString Filename = FPaths::GetBaseFilename(FullPath).ToLower();
        
        if (Filename.Equals(NormalizedSearch))
        {
            UE_LOG(LogTemp, Display, TEXT("   ✅ Exact Match: %s"), *FullPath);
            return FullPath;
        }
    }

    // Strategy 2: Substring Match
    // e.g. Input: "Fire" -> Matches "/Game/VFX/NS_BlueFire.NS_BlueFire"
    for (const FString& FullPath : DiscoveredParticleNames)
    {
        FString Filename = FPaths::GetBaseFilename(FullPath).ToLower();
        
        if (Filename.Contains(NormalizedSearch))
        {
            UE_LOG(LogTemp, Display, TEXT("   ✅ Substring Match: %s"), *FullPath);
            return FullPath;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("   ❌ Particle not found: %s"), *SearchName);
    return TEXT("");
}


void UAssetIndexer::AuditTexture(const FString& SearchTerm)
{
    UE_LOG(LogTemp, Warning, TEXT("🔍 AUDIT: Searching database for '%s'"), *SearchTerm);
    
    // 1. Test Analysis
    FParsedTextureInfo Info = AnalyzeTexturePath(SearchTerm);
    UE_LOG(LogTemp, Warning, TEXT("   Analyzer sees: Original='%s' -> Base='%s' Type=%d"), 
        *Info.OriginalPath, *Info.BaseName, (uint8)Info.Type);

    // 2. Test Exact Lookup
    FString SearchBase = Info.BaseName.ToLower();
    if (MaterialDatabase.Contains(SearchBase))
    {
        const FTextureSet& Set = MaterialDatabase[SearchBase];
        UE_LOG(LogTemp, Warning, TEXT("   ✅ DATABASE HIT for key '%s':"), *SearchBase);
        UE_LOG(LogTemp, Display, TEXT("      BaseColor: %s"), *Set.BaseColorPath);
        UE_LOG(LogTemp, Display, TEXT("      Normal:    %s"), *Set.NormalPath);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("   ❌ DATABASE MISS for key '%s'"), *SearchBase);
    }

    // 3. Dump similar keys
    UE_LOG(LogTemp, Display, TEXT("   --- Similar Keys in DB ---"));
    int32 Count = 0;
    for (const auto& Pair : MaterialDatabase)
    {
        if (Pair.Key.Contains(SearchBase) && Count < 5)
        {
            UE_LOG(LogTemp, Display, TEXT("      Found: %s"), *Pair.Key);
            Count++;
        }
    }
}


// AssetIndexer.cpp

void UAssetIndexer::BatchResolveTextures(FEnhancedScenePlan& Plan)
{
    UE_LOG(LogTemp, Display, TEXT("🔄 AssetIndexer: Batch resolving textures for %d props..."), Plan.Props.Num());

    for (FPropsModification& Prop : Plan.Props)
    {
        // 1. Check if we have a key to search for
        FString TextureKey = Prop.Texture.BaseColorPath;
        if (TextureKey.IsEmpty()) continue;

        // 2. Perform the lookup (Self-call)
        FTextureSet ResolvedSet = ResolveTextureFromName(TextureKey);

        // 3. Logic: If BaseColor is missing but we found other maps (e.g. only Normal map exists)
        bool bHasAnyMap = !ResolvedSet.BaseColorPath.IsEmpty() || 
                          !ResolvedSet.NormalPath.IsEmpty() || 
                          !ResolvedSet.RoughnessPath.IsEmpty();

        if (bHasAnyMap)
        {
            // Apply the set
            Prop.Texture = ResolvedSet; 
        }
        else
        {
            // Nothing found. Clear the invalid string so we don't try to load "Grass".
            UE_LOG(LogTemp, Warning, TEXT("  ⚠️  Texture '%s' not found - Clearing property"), *TextureKey);
            Prop.Texture.BaseColorPath.Empty(); 
            Prop.Texture.NormalPath.Empty();
            Prop.Texture.RoughnessPath.Empty();
            Prop.Texture.MetallicPath.Empty();
            Prop.Texture.AOPath.Empty();
        }
    }
}

void UAssetIndexer::BatchResolveMeshes(FEnhancedScenePlan& Plan)
{
    UE_LOG(LogTemp, Display, TEXT("🔄 AssetIndexer: Batch resolving meshes for %d spawns..."), Plan.SpawnRequest.Num());
    
    int32 Resolved = 0, Failed = 0;

    for (FSpawnRequest& Spawn : Plan.SpawnRequest)
    {
        if (Spawn.AssetPath.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("  ⚠️  Empty AssetPath for '%s'"), *Spawn.ObjectName);
            Failed++;
            continue;
        }

        // Use the Robust Variant Resolver we wrote earlier
        FString ResolvedMesh = ResolveMeshToFullPathWithVariants(Spawn.AssetPath);
        
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
        UE_LOG(LogTemp, Warning, TEXT("  ⚠️  BatchResolveMeshes: %d/%d resolved"), Resolved, Resolved + Failed);
    }
}

void UAssetIndexer::BatchResolveParticles(FEnhancedScenePlan& Plan)
{
    for (FSpawnRequest& Req : Plan.ParticleSpawns)
    {
        if (Req.AssetPath.IsEmpty()) continue;

        // STRICT: Only look for particles
        FString Path = ResolveParticlePath(Req.AssetPath);

        if (!Path.IsEmpty())
        {
            Req.AssetPath = Path;
            UE_LOG(LogTemp, Display, TEXT("   ✨ Resolved Particle: %s"), *Req.AssetPath);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("   ❌ Particle Not Found: %s"), *Req.AssetPath);
        }
    }
}

void UAssetIndexer::ResolveEnvironmentAssets(FEnhancedScenePlan& Plan)
{
    if (Plan.Environment.PostProcessingName.IsEmpty()) return;

    FString FullPath = ResolvePostProcessPath(Plan.Environment.PostProcessingName);
    
    if (!FullPath.IsEmpty())
    {
        Plan.Environment.PostProcessingName = FullPath;
    }
}


// =========================================================================
// NEW PRUNING LOGIC
// =========================================================================

TArray<FString> UAssetIndexer::GetTopKMeshesForQuery(const FString& SearchQuery, int32 MaxResults)
{
    TArray<FString> Results;
    
    // 1. Get ALL semantic matches using our existing logic (Groups + Keywords + Substrings)
    TArray<FString> AllMatches = ResolveAllMeshPaths(SearchQuery);
    
    // 2. If we found fewer than K, return all of them
    if (AllMatches.Num() <= MaxResults)
    {
        return AllMatches;
    }
    
    // 3. Selection Strategy: Prioritize Diversity if it's a large group
    // If we matched a "Chair" group with 20 items, we want items [0], [mid], [end] or randoms
    // to give the LLM variety, rather than just Chair_01, Chair_02...
    
    // For now, let's just pick K randoms to avoid bias, or pick top K based on string length/complexity
    // Random shuffle is usually best for generative variety
    
    // Perform Fisher-Yates shuffle
    int32 n = AllMatches.Num();
    for (int32 i = n - 1; i > 0; i--)
    {
        int32 j = FMath::RandRange(0, i);
        AllMatches.Swap(i, j);
    }
    
    // Take Top K
    for (int32 i = 0; i < MaxResults; i++)
    {
        Results.Add(AllMatches[i]);
    }
    
    return Results;
}

TArray<FString> UAssetIndexer::GetTopKTexturesForQuery(const FString& SearchQuery, int32 MaxResults)
{
    TArray<FString> Results;
    FString NormalizedSearch = SearchQuery.ToLower();
    
    // 1. Exact/Substring scan of MaterialDatabase keys
    TArray<FString> MatchingKeys;
    
    for (const auto& Pair : MaterialDatabase)
    {
        if (Pair.Key.ToLower().Contains(NormalizedSearch) || NormalizedSearch.Contains(Pair.Key.ToLower()))
        {
            MatchingKeys.Add(Pair.Key);
        }
    }
    
    // 2. Fuzzy fallback if empty
    if (MatchingKeys.Num() == 0)
    {
        for (const auto& Pair : MaterialDatabase)
        {
            if (CalculateSimilarity(Pair.Key.ToLower(), NormalizedSearch) > 50)
            {
                MatchingKeys.Add(Pair.Key);
            }
        }
    }
    
    // 3. Prune to Top K
    if (MatchingKeys.Num() > MaxResults)
    {
        // Shuffle
        int32 n = MatchingKeys.Num();
        for (int32 i = n - 1; i > 0; i--)
        {
            int32 j = FMath::RandRange(0, i);
            MatchingKeys.Swap(i, j);
        }
        MatchingKeys.SetNum(MaxResults);
    }
    
    // 4. Resolve to Strings (we return the BaseName so the LLM can use it in the JSON)
    // NOTE: The Master Planner expects "Valid Materials" list.
    // If we return Full Paths, the LLM might get confused if the schema expects Base Names.
    // Let's return the Base Names (Keys) which are what ResolveBaseMaterialToTextureSet expects.
    return MatchingKeys; 
}