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
    AsyncTask(ENamedThreads::GameThread, [this, ScanPath]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning textures in %s"), *ScanPath);
        ScanAssetsOfType(UTexture2D::StaticClass(), ScanPath, DiscoveredTextureNames);
        CheckAllScansComplete();
    });
}

void UAssetIndexer::ScanForParticlesAsync(FString ScanPath)
{
    AsyncTask(ENamedThreads::GameThread, [this, ScanPath]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning Niagara particles in %s"), *ScanPath);
        
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        
        FARFilter Filter;
        // Specifically look for Niagara Systems
        Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
        Filter.PackagePaths.Add(FName(*ScanPath));
        Filter.bRecursivePaths = true;

        TArray<FAssetData> AssetDataArray;
        AssetRegistryModule.Get().GetAssets(Filter, AssetDataArray);

        DiscoveredParticleNames.Empty();

        for (const FAssetData& AssetData : AssetDataArray)
        {
            // CRITICAL FIX: Get the FULL PATH, not just the name.
            // Example: /Game/Particles/Fire/NS_Fire.NS_Fire
            FString FullPath = AssetData.GetSoftObjectPath().ToString();
            
            DiscoveredParticleNames.Add(FullPath);
            UE_LOG(LogTemp, Verbose, TEXT("   Found Particle: %s"), *FullPath);
        }
        
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d Niagara systems"), DiscoveredParticleNames.Num());

        CheckAllScansComplete();
    });
}

void UAssetIndexer::ScanForPostProcessMaterialsAsync(FString ScanPath)
{
    AsyncTask(ENamedThreads::GameThread, [this, ScanPath]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning post-process materials in %s"), *ScanPath);
        ScanAssetsOfType(UMaterialInterface::StaticClass(), ScanPath, DiscoveredPostProcessNames);
        CheckAllScansComplete();
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

    // 1. Exact Match
    if (MaterialDatabase.Contains(SearchName))
    {
        return MaterialDatabase[SearchName];
    }

    // 2. Fuzzy Search
    FString CleanSearch = SearchName.ToLower().Replace(TEXT("_"), TEXT("")).Replace(TEXT(" "), TEXT(""));

    for (const auto& Pair : MaterialDatabase)
    {
        FString CleanKey = Pair.Key.ToLower().Replace(TEXT("_"), TEXT(""));
        
        if (CleanKey.Equals(CleanSearch) || CleanKey.Contains(CleanSearch) || CleanSearch.Contains(CleanKey))
        {
            UE_LOG(LogTemp, Display, TEXT("ResolveTextureFromName: Fuzzy match '%s' -> '%s'"), *SearchName, *Pair.Key);
            return Pair.Value;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("ResolveTextureFromName: No match for '%s'"), *SearchName);
    return Result;
}

void UAssetIndexer::ScanForStaticMeshesAsync(FString ScanPath)
{
    AsyncTask(ENamedThreads::GameThread, [this, ScanPath]()
    {
        UE_LOG(LogTemp, Display, TEXT("🔍 Scanning static meshes in: %s"), *ScanPath);
        
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        FARFilter Filter;
        Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
        Filter.PackagePaths.Add(FName(*ScanPath));
        Filter.bRecursivePaths = true;
        
        TArray<FAssetData> AssetDataArray;
        AssetRegistryModule.Get().GetAssets(Filter, AssetDataArray);
        
        UE_LOG(LogTemp, Display, TEXT("   Found %d static meshes"), AssetDataArray.Num());
        
        for (const FAssetData& AssetData : AssetDataArray)
        {
            FString MeshName = AssetData.AssetName.ToString();
            
            // ✅ CRITICAL FIX: Use GetSoftObjectPath() which gives CORRECT path
            FString FullPath = AssetData.GetSoftObjectPath().ToString();
            
            // Verify path doesn't have double slashes
            if (FullPath.Contains(TEXT("//")))
            {
                UE_LOG(LogTemp, Error, TEXT("❌ Invalid path detected: %s"), *FullPath);
                continue; // Skip this asset
            }
            
            FString Directory = FPaths::GetPath(FullPath);
            
            FMeshAssetInfo MeshInfo;
            MeshInfo.MeshName = MeshName;
            MeshInfo.MeshAsset = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(FullPath));
            MeshInfo.FullPath = FullPath; // Store clean path
            MeshInfo.Directory = Directory;
            MeshInfo.Keywords = ExtractKeywordsFromMesh(MeshName);
            
            FString NormalizedName = NormalizeName(MeshName);
            DiscoveredMeshes.Add(NormalizedName, MeshInfo);
            DiscoveredStaticMeshNames.AddUnique(MeshName);
            
            UE_LOG(LogTemp, Verbose, TEXT("   ✓ %s -> %s"), *MeshName, *FullPath);
        }
        
        // Build variant groups
        for (const auto& MeshPair : DiscoveredMeshes)
        {
            FString BaseName = ExtractBaseName(MeshPair.Value.MeshName);
            FString NormalizedBase = NormalizeName(BaseName);
            
            if (!VariantGroups.Contains(NormalizedBase))
            {
                FMeshVariantGroup Group;
                Group.BaseName = BaseName;
                VariantGroups.Add(NormalizedBase, Group);
            }
            
            VariantGroups[NormalizedBase].Variants.AddUnique(MeshPair.Value.MeshName);
            VariantGroups[NormalizedBase].VariantPaths.AddUnique(MeshPair.Value.FullPath);
        }
        
        int32 VariantGroupCount = 0;
        for (const auto& Group : VariantGroups)
        {
            if (Group.Value.Variants.Num() > 1) VariantGroupCount++;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("✅ Mesh scan: %d meshes, %d variant groups"), 
            DiscoveredMeshes.Num(), VariantGroupCount);
        
        CheckAllScansComplete();
    });
}


FParsedTextureInfo UAssetIndexer::AnalyzeTexturePath(const FString& FullPath)
{
    FParsedTextureInfo Info;
    Info.OriginalPath = FullPath;
    Info.Type = ETextureMapType::Unknown;

    FString Filename = FPaths::GetBaseFilename(FullPath);
    FString ProcessedName = Filename.Replace(TEXT("-"), TEXT("_")); 

    TArray<FString> Tokens;
    ProcessedName.ParseIntoArray(Tokens, TEXT("_"), true);

    // Dictionaries
    const TSet<FString> IgnorePrefixes = { TEXT("T"), TEXT("Tex"), TEXT("M"), TEXT("Mat"), TEXT("SM") };
    
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
        
        // ✅ NEW: Displacement
        {TEXT("Disp"), ETextureMapType::Displacement}, {TEXT("Displacement"), ETextureMapType::Displacement},
        {TEXT("Height"), ETextureMapType::Displacement}, {TEXT("H"), ETextureMapType::Displacement},

        // ✅ NEW: Opacity
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
        
        if (i == 0 && IgnorePrefixes.Contains(Token)) continue;

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

        if (Token.Equals("2k", ESearchCase::IgnoreCase) || 
            Token.Equals("4k", ESearchCase::IgnoreCase) ||
            Token.Equals("8k", ESearchCase::IgnoreCase)) continue;

        ContentTokens.Add(Token);
    }

    Info.BaseName = FString::Join(ContentTokens, TEXT("_"));
    if (Info.BaseName.IsEmpty()) Info.BaseName = Filename;

    return Info;
}
// In AssetIndexer.cpp

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
    if (SearchName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("ResolveMesh: Empty search name"));
        return TEXT("");
    }
    
    FString NormalizedSearch = NormalizeName(SearchName);
    
    UE_LOG(LogTemp, Display, TEXT("🔎 ResolveMesh: Searching for '%s'"), *SearchName);
    
    // ========================================
    // STRATEGY 1: Exact normalized match
    // ========================================
    if (DiscoveredMeshes.Contains(NormalizedSearch))
    {
        FString Result = DiscoveredMeshes[NormalizedSearch].FullPath;
        UE_LOG(LogTemp, Display, TEXT("   ✅ [EXACT] Found: %s"), *Result);
        return Result;
    }
    
    // ========================================
    // STRATEGY 2: Substring match (case insensitive)
    // ========================================
    for (const auto& Pair : DiscoveredMeshes)
    {
        if (Pair.Value.MeshName.Contains(SearchName, ESearchCase::IgnoreCase) ||
            NormalizedSearch.Contains(Pair.Key))
        {
            FString Result = Pair.Value.FullPath;
            UE_LOG(LogTemp, Display, TEXT("   ✅ [SUBSTRING] '%s' in '%s' -> %s"), 
                *SearchName, *Pair.Value.MeshName, *Result);
            return Result;
        }
    }
    
    // ========================================
    // STRATEGY 3: Keyword matching
    // ========================================
    TArray<FString> SearchKeywords = ExtractKeywordsFromMesh(SearchName);
    
    for (const auto& Pair : DiscoveredMeshes)
    {
        for (const FString& SearchKeyword : SearchKeywords)
        {
            for (const FString& AssetKeyword : Pair.Value.Keywords)
            {
                if (SearchKeyword.Equals(AssetKeyword, ESearchCase::IgnoreCase))
                {
                    FString Result = Pair.Value.FullPath;
                    UE_LOG(LogTemp, Display, TEXT("   ✅ [KEYWORD] '%s' matches '%s' (keyword: %s)"), 
                        *SearchName, *Pair.Value.MeshName, *AssetKeyword);
                    return Result;
                }
            }
        }
    }
    
    // ========================================
    // STRATEGY 4: Similarity-based fuzzy match (60%+)
    // ========================================
    FString BestMatch;
    int32 BestScore = 0;
    
    for (const auto& Pair : DiscoveredMeshes)
    {
        int32 Score = CalculateSimilarity(NormalizedSearch, Pair.Key);
        if (Score > BestScore && Score > 60)
        {
            BestScore = Score;
            BestMatch = Pair.Value.FullPath;
        }
    }
    
    if (!BestMatch.IsEmpty())
    {
        UE_LOG(LogTemp, Display, TEXT("   ⚠️  [FUZZY %d%%] %s -> %s"), 
            BestScore, *SearchName, *BestMatch);
        return BestMatch;
    }
    
    // ========================================
    // STRATEGY 5: Random from category/prefix
    // ========================================
    TArray<FString> CandidatePaths;
    for (const auto& Pair : DiscoveredMeshes)
    {
        if (Pair.Value.MeshName.StartsWith(TEXT("SM_")))
        {
            CandidatePaths.Add(Pair.Value.FullPath);
        }
    }
    
    if (CandidatePaths.Num() > 0)
    {
        int32 RandomIdx = FMath::RandRange(0, CandidatePaths.Num() - 1);
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️  [FALLBACK RANDOM] Using random SM_* mesh"));
        return CandidatePaths[RandomIdx];
    }
    
    UE_LOG(LogTemp, Error, TEXT("❌ ResolveMesh: Could not find mesh for '%s'"), *SearchName);
    return TEXT("");
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
