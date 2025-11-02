// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h"
#include "AssetIndexer.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAssetScanComplete);

UCLASS()
class PROJECT_CANVAS_API UAssetIndexer : public UObject
{
    GENERATED_BODY()

public:
    // === SCAN FUNCTIONS ===
    
    /** Kicks off an async scan for ALL asset types */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    void ScanAllAssetsAsync(UWorld* WorldContext);
    
    /** Scan only textures (your current implementation) */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    void ScanForTexturesAsync(FString ScanPath = TEXT("/Game/DATABASE/textures"));
    
    /** Scan for particle systems */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    void ScanForParticlesAsync(FString ScanPath = TEXT("/Game/DATABASE/particles"));
    
    /** Scan for post-process materials */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    void ScanForPostProcessMaterialsAsync(FString ScanPath = TEXT("/Game/DATABASE/postprocess"));
    
    /** Scan for actor tags in the current level */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    void ScanActorTagsInLevel(UWorld* WorldContext);

    // === GETTERS ===
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    TArray<FString> GetDiscoveredTextureNames() const { return DiscoveredTextureNames; }
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    TArray<FString> GetDiscoveredParticleNames() const { return DiscoveredParticleNames; }
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    TArray<FString> GetDiscoveredPostProcessNames() const { return DiscoveredPostProcessNames; }
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    TArray<FString> GetDiscoveredActorTags() const { return DiscoveredActorTags; }
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    bool IsScanComplete() const { return bIsScanComplete; }

    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    FTextureSet ResolveBaseMaterialToTextureSet(const FString& BaseMaterialName);
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    TMap<FString, FTextureSet> BuildMaterialDatabase();
    // === DELEGATE ===
    
    UPROPERTY(BlueprintAssignable, Category = "AssetIndexer")
    FOnAssetScanComplete OnScanComplete;
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    TArray<FString> GetMaterialBaseNames() const
    {
        TArray<FString> Names;
        MaterialDatabase.GetKeys(Names);
        return Names;
    }

    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    bool bIsNameMatch(FString Key,FString AssetName);

    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    FTextureSet ResolveTextureFromName(const FString& SearchName);
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    void ScanForStaticMeshesAsync(FString ScanPath = TEXT("/Game/DATABASE/meshes"));

    // ... existing getters
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    TArray<FString> GetDiscoveredStaticMeshNames() const { return DiscoveredStaticMeshNames; }
private:
    // Internal scan counter
    int32 PendingScans = 0;
    FString ExtractMaterialBaseName(const FString& TextureName);
    void CheckAllScansComplete();
    void ScanAssetsOfType(const UClass* AssetClass, FString ScanPath, TArray<FString>& OutArray);

    // === DATA ===
    
    UPROPERTY()
    TArray<FString> DiscoveredTextureNames;
    
    UPROPERTY()
    TArray<FString> DiscoveredParticleNames;
    
    UPROPERTY()
    TArray<FString> DiscoveredPostProcessNames;
    
    UPROPERTY()
    TArray<FString> DiscoveredActorTags;
    UPROPERTY()
    TArray<FString> DiscoveredStaticMeshNames;
    UPROPERTY()
    TMap<FString, FTextureSet> MaterialDatabase;
    UPROPERTY()
    bool bIsScanning = false;
    
    UPROPERTY()
    bool bIsScanComplete = false;
};

