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
void UAssetIndexer::ScanAllAssetsAsync(UWorld* WorldContext)
{
    if (bIsScanning)
    {
        UE_LOG(LogTemp, Warning, TEXT("AssetIndexer: Scan already in progress."));
        return;
    }

    bIsScanning = true;
    bIsScanComplete = false;
    PendingScans = 0;
    
    // Clear old data
    DiscoveredTextureNames.Empty();
    DiscoveredParticleNames.Empty();
    DiscoveredPostProcessNames.Empty();
    DiscoveredActorTags.Empty();
    DiscoveredStaticMeshNames.Empty();
    UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Starting comprehensive asset scan..."));

    // Launch all scans
    PendingScans = 5; // Textures, Particles, PostProcess, ActorTags
    
    ScanForTexturesAsync(TEXT("/Game/DATABASE/textures"));
    ScanForParticlesAsync(TEXT("/Game/DATABASE/particles"));
    ScanForPostProcessMaterialsAsync(TEXT("/Game/DATABASE/postprocess"));
    ScanForStaticMeshesAsync(TEXT("/Game/DATABASE/meshes"));
    ScanActorTagsInLevel(WorldContext);
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
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning particles in %s"), *ScanPath);
        ScanAssetsOfType(UParticleSystem::StaticClass(), ScanPath, DiscoveredParticleNames);
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

    AsyncTask(ENamedThreads::GameThread, [this, WorldContext]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning actor tags in current level..."));
        
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(WorldContext, AActor::StaticClass(), AllActors);
        
        TSet<FString> UniqueTagsSet; // Use set to avoid duplicates
        
        for (AActor* Actor : AllActors)
        {
            if (Actor)
            {
                for (const FName& Tag : Actor->Tags)
                {
                    // Only add tags that follow your naming convention
                    FString TagString = Tag.ToString();
                    if (TagString.Contains(".")) // e.g., "Background.Wall"
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
        OutArray.Add(AssetData.AssetName.ToString());
    }

    UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d assets of type %s"), 
           OutArray.Num(), *AssetClass->GetName());
}

void UAssetIndexer::CheckAllScansComplete()
{
    PendingScans--;
    
    if (PendingScans <= 0)
    {
        bIsScanning = false;
        bIsScanComplete = true;
        
        MaterialDatabase = BuildMaterialDatabase();
        UE_LOG(LogTemp, Warning, TEXT("AssetIndexer: Built material database with %d unique materials"), 
            MaterialDatabase.Num());
        
        UE_LOG(LogTemp, Warning, TEXT("AssetIndexer: ========== SCAN COMPLETE =========="));
        UE_LOG(LogTemp, Warning, TEXT("  Textures: %d"), DiscoveredTextureNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  Particles: %d"), DiscoveredParticleNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  PostProcess: %d"), DiscoveredPostProcessNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  StaticMeshes: %d"), DiscoveredStaticMeshNames.Num()); 
        UE_LOG(LogTemp, Warning, TEXT("  Actor Tags: %d"), DiscoveredActorTags.Num());
        UE_LOG(LogTemp, Warning, TEXT("================================================"));
        
        OnScanComplete.Broadcast();
    }
}


TMap<FString, FTextureSet> UAssetIndexer::BuildMaterialDatabase()
{
    TMap<FString, FTextureSet> Database;

    // Define token lists for each texture type
    const TArray<FString> DiffuseTokens     = { "_diff_", "_basecolor", "_albedo", "_color", "_d" };
    const TArray<FString> RoughnessTokens   = { "_rough_", "_rgh", "_roughness", "_r","_ORM","_orm","ao_r_mt"  };
    const TArray<FString> NormalTokens      = { "_nor_", "_normal", "_n" };
    const TArray<FString> MetallicTokens    = { "_metal_", "_metallic", "_m","_ORM","_orm","AO_R_MT","ao_r_mt"  };
    const TArray<FString> AOTokens          = { "_ao_", "_ambient", "_a","AO_R_MT","ao_r_mt" };

    for (const FString& TextureName : DiscoveredTextureNames)
    {
        FString BaseName = ExtractMaterialBaseName(TextureName);
        if (BaseName.IsEmpty())
            continue;

        if (!Database.Contains(BaseName))
            Database.Add(BaseName, FTextureSet());

        FTextureSet& Set = Database[BaseName];
        const FString Lower = TextureName.ToLower();

        auto MatchAny = [&Lower](const TArray<FString>& Tokens) -> bool
        {
            for (const FString& Token : Tokens)
            {
                if (Lower.Contains(Token))
                    return true;
            }
            return false;
        };

        if (MatchAny(DiffuseTokens))
            Set.BaseColorPath = TextureName;
        else if (MatchAny(RoughnessTokens))
            Set.RoughnessPath = TextureName;
        else if (MatchAny(NormalTokens))
            Set.NormalPath = TextureName;
        else if (MatchAny(MetallicTokens))
            Set.MetallicPath = TextureName;
        else if (MatchAny(AOTokens))
            Set.AOPath = TextureName;
    }

    UE_LOG(LogTemp, Log, TEXT("BuildMaterialDatabase completed. Indexed %d materials."), Database.Num());
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
    TArray<FString> Suffixes = {
        TEXT("_diff_2k"), TEXT("_diff_4k"),
        TEXT("_rough_2k"), TEXT("_rough_4k"),
        TEXT("_nor_gl_2k"), TEXT("_nor_gl_4k"),
        TEXT("_nor_dx_2k"), TEXT("_nor_dx_4k"),
        TEXT("_metal_2k"), TEXT("_metal_4k"),
        TEXT("_arm_2k"), TEXT("_arm_4k"),
        TEXT("_ao_2k"), TEXT("_ao_4k"),
        TEXT("_disp_2k"), TEXT("_disp_4k"),
        TEXT("_spec_ior_2k"), TEXT("_spec_ior_4k"),
        TEXT("_anisotropy_strength_2k"), TEXT("_anisotropy_strength_4k"),
        TEXT("_anisotropy_rotation_2k"), TEXT("_anisotropy_rotation_4k"),
        TEXT("_mask_2k"), TEXT("_mask_4k"),
        TEXT("_Normal"), TEXT("_BaseColor"),
        TEXT("_Occlusion"),TEXT("_Roughness"),
        TEXT("_Metal"), TEXT("_Ao"), TEXT("_Displacement"),
        TEXT("_Metallic"), TEXT("_AO"), TEXT("_Displacement_AO"),
        TEXT("_Metallic_AO"), TEXT("_OcclusionRoughnessMetallic"),
        TEXT("_BC"),TEXT("_ORM"),TEXT("_BC"),TEXT("AO_R_MT")
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


bool UAssetIndexer::bIsNameMatch(FString Key, FString AssetName)
{
    FString CleanKey = Key.ToLower().Replace(TEXT("_"), TEXT(""));
    FString CleanAsset = AssetName.ToLower().Replace(TEXT("_"), TEXT(""));
    return CleanAsset.Contains(CleanKey);
}

FTextureSet UAssetIndexer::ResolveTextureFromName(const FString& SearchName)
{
    FTextureSet Result;

    FString CleanKey = SearchName.ToLower().Replace(TEXT("_"), TEXT(""));
    for (const auto& Pair : MaterialDatabase)
    {
        FString CleanAsset = Pair.Key.ToLower().Replace(TEXT("_"), TEXT(""));
        if (CleanAsset.Contains(CleanKey))
        {
            // Found partial match; copy this material set
            Result = Pair.Value;
            UE_LOG(LogTemp, Display, TEXT("ResolveTextureFromName: matched %s -> %s"), *SearchName, *Pair.Key);
            return Result;
        }
    }

    // fallback: partial/fuzzy match by token
    for (const auto& Pair : MaterialDatabase)
    {
        if (bIsNameMatch(SearchName, Pair.Key))
        {
            Result = Pair.Value;
            UE_LOG(LogTemp, Display, TEXT("ResolveTextureFromName (fuzzy) %s -> %s"), *SearchName, *Pair.Key);
            return Result;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("No texture match found for %s"), *SearchName);
    return Result; // Empty set
}
void UAssetIndexer::ScanForStaticMeshesAsync(FString ScanPath)
{
    AsyncTask(ENamedThreads::GameThread, [this, ScanPath]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning static meshes in %s"), *ScanPath);
        ScanAssetsOfType(UStaticMesh::StaticClass(), ScanPath, DiscoveredStaticMeshNames);
        CheckAllScansComplete();
    });
}

FString UAssetIndexer::ResolveStaticMeshName(const FString& SearchName)
{
    if (SearchName.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("ResolveStaticMeshName: Empty search name"));
        return TEXT("");
    }

    // 1. Try exact match (case-insensitive)
    for (const FString& MeshName : DiscoveredStaticMeshNames)
    {
        if (MeshName.Equals(SearchName, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogTemp, Display, TEXT("ResolveStaticMeshName: Exact match '%s' -> '%s'"), 
                *SearchName, *MeshName);
            return MeshName;
        }
    }

    // 2. Try contains match (e.g., "Rock" finds "SM_Rock_Large")
    for (const FString& MeshName : DiscoveredStaticMeshNames)
    {
        if (MeshName.Contains(SearchName, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogTemp, Display, TEXT("ResolveStaticMeshName: Contains match '%s' -> '%s'"), 
                *SearchName, *MeshName);
            return MeshName;
        }
    }

    // 3. Try fuzzy match using bIsNameMatch helper
    for (const FString& MeshName : DiscoveredStaticMeshNames)
    {
        if (bIsNameMatch(SearchName, MeshName))
        {
            UE_LOG(LogTemp, Display, TEXT("ResolveStaticMeshName: Fuzzy match '%s' -> '%s'"), 
                *SearchName, *MeshName);
            return MeshName;
        }
    }

    // 4. No match found
    UE_LOG(LogTemp, Warning, TEXT("ResolveStaticMeshName: No match found for '%s'"), *SearchName);
    return TEXT("");
}
