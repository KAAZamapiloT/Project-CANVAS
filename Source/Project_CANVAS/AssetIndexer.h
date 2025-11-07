// AssetIndexer.h - FINAL CLEAN VERSION

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h"
#include "AssetIndexer.generated.h"

// ========================================
// STRUCTS
// ========================================

/**
 * Mesh asset information with full path and keywords
 */
USTRUCT(BlueprintType)
struct FMeshAssetInfo
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString MeshName;
    
    UPROPERTY()
    FString FullPath;
    
    UPROPERTY()
    FString Directory;
    
    UPROPERTY()
    TArray<FString> Keywords;
};

/**
 * Group of mesh variants (e.g., Chair_01, Chair_02, Chair_03)
 */
USTRUCT(BlueprintType)
struct FMeshVariantGroup
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString BaseName;
    
    UPROPERTY()
    TArray<FString> Variants;
    
    UPROPERTY()
    TArray<FString> VariantPaths;
    
    FString GetRandomVariant() const
    {
        if (Variants.Num() == 0) return TEXT("");
        return Variants[FMath::RandRange(0, Variants.Num() - 1)];
    }
    
    FString GetRandomVariantPath() const
    {
        if (VariantPaths.Num() == 0) return TEXT("");
        return VariantPaths[FMath::RandRange(0, VariantPaths.Num() - 1)];
    }
};

// ========================================
// DELEGATES
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAssetScanComplete);

// ========================================
// CLASS DECLARATION
// ========================================

/**
 * AssetIndexer - Comprehensive asset discovery and resolution
 * 
 * Responsibilities:
 * - Scan all asset types (meshes, textures, particles, post-process)
 * - Build searchable databases
 * - Resolve asset names robustly (fuzzy matching, variants, keywords)
 * - Support variant selection with randomness
 * 
 * Design: Async scanning, on-demand queries
 */
UCLASS()
class PROJECT_CANVAS_API UAssetIndexer : public UObject
{
    GENERATED_BODY()

public:
    
    // ========================================
    // SCANNING FUNCTIONS
    // ========================================
    
    /// Scan all asset types (async, non-blocking)
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanAllAssetsAsync(UWorld* WorldContext);
    
    /// Scan textures only
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanForTexturesAsync(FString ScanPath = TEXT("/Game/DATABASE/textures"));
    
    /// Scan particles only
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanForParticlesAsync(FString ScanPath = TEXT("/Game/DATABASE/particles"));
    
    /// Scan post-process materials only
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanForPostProcessMaterialsAsync(FString ScanPath = TEXT("/Game/DATABASE/postprocess"));
    
    /// Scan static meshes (NEW - with variant support)
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanForStaticMeshesAsync(FString ScanPath = TEXT("/Game/DATABASE/meshes"));
    
    /// Scan actor tags in current level
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanActorTagsInLevel(UWorld* WorldContext);
    
    // ========================================
    // TEXTURE QUERIES
    // ========================================
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    TArray<FString> GetDiscoveredTextureNames() const { return DiscoveredTextureNames; }
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    FTextureSet ResolveTextureFromName(const FString& SearchName);
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    FTextureSet ResolveBaseMaterialToTextureSet(const FString& BaseMaterialName);
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    TArray<FString> GetMaterialBaseNames() const
    {
        TArray<FString> Names;
        MaterialDatabase.GetKeys(Names);
        return Names;
    }
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    TMap<FString, FTextureSet> BuildMaterialDatabase();
    
    // ========================================
    // PARTICLE QUERIES
    // ========================================
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Particles")
    TArray<FString> GetDiscoveredParticleNames() const { return DiscoveredParticleNames; }
    
    // ========================================
    // POST-PROCESS QUERIES
    // ========================================
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|PostProcess")
    TArray<FString> GetDiscoveredPostProcessNames() const { return DiscoveredPostProcessNames; }
    
    // ========================================
    // ACTOR TAG QUERIES
    // ========================================
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Tags")
    TArray<FString> GetDiscoveredActorTags() const { return DiscoveredActorTags; }
    
    // ========================================
    // MESH RESOLUTION (NEW - PRIMARY)
    // ========================================
    
    /**
     * Resolve mesh with variant support (picks random variant)
     * 
     * Strategies:
     * 1. Exact numbered variant (e.g., "Chair_02")
     * 2. Variant group random pick (e.g., "Chair" → one of 3)
     * 3. Robust single-mesh resolution
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    FString ResolveMeshToFullPathWithVariants(const FString& SearchName);
    
    /**
     * Resolve mesh name robustly (no variants, single result)
     * 
     * Strategies:
     * 1. Exact normalized match
     * 2. Substring match
     * 3. Keyword semantic match
     * 4. Fuzzy match (60%+)
     * 5. Random SM_* fallback
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    FString ResolveMeshToFullPath(const FString& SearchName);
    
    // ========================================
    // MESH VARIANT QUERIES
    // ========================================
    
    /// Get all variants of a mesh
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    FMeshVariantGroup GetMeshVariants(const FString& SearchName);
    
    /// Count variants for a mesh
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    int32 GetVariantCount(const FString& SearchName);
    
    // ========================================
    // MESH INFO QUERIES
    // ========================================
    
    /// Get detailed info about a mesh
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    FMeshAssetInfo GetMeshInfo(const FString& SearchName);
    
    /// Get all discovered mesh names
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    TArray<FString> GetAllMeshNames() const;

    // AssetIndexer.h - Add this function:
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    TArray<FString> ResolveAllMeshPaths(const FString& SearchName);

    /// Get total mesh count
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    int32 GetMeshCount() const { return DiscoveredMeshes.Num(); }
    
    // ========================================
    // DEBUG & LOGGING
    // ========================================
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Debug")
    void PrintAllMeshes() const;
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Debug")
    void PrintMeshVariants() const;
    
    // ========================================
    // STATE
    // ========================================
    
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    bool IsScanComplete() const { return bIsScanComplete; }
    
    UPROPERTY(BlueprintAssignable, Category = "AssetIndexer")
    FOnAssetScanComplete OnScanComplete;

private:
    
    // ========================================
    // MESH STORAGE (NEW)
    // ========================================
    
    /// Main mesh database
    UPROPERTY()
    TMap<FString, FMeshAssetInfo> DiscoveredMeshes;
    
    /// Variant groups (e.g., "chair" → 3 variants)
    UPROPERTY()
    TMap<FString, FMeshVariantGroup> VariantGroups;
    
    /// Quick lookup index
    UPROPERTY()
    TArray<FString> MeshNameCache;
   
    
    // ========================================
    // OTHER ASSET STORAGE (EXISTING)
    // ========================================
    
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
    
    // ========================================
    // STATE
    // ========================================
    
    UPROPERTY()
    bool bIsScanning = false;
    
    UPROPERTY()
    bool bIsScanComplete = false;
    
    int32 PendingScans = 0;
    
    // ========================================
    // INTERNAL HELPERS
    // ========================================
    
    // Scanning
    void ScanAssetsOfType(const UClass* AssetClass, FString ScanPath, TArray<FString>& OutArray);
    void CheckAllScansComplete();
    
    // Mesh name processing
    FString ExtractBaseName(const FString& VariantName);
    bool IsNumberedVariant(const FString& Name, int32& OutVariantNumber);
    FString NormalizeName(const FString& Name);
    TArray<FString> ExtractKeywordsFromMesh(const FString& MeshName);
    int32 CalculateSimilarity(const FString& A, const FString& B);
    
    // Material processing
    FString ExtractMaterialBaseName(const FString& TextureName);
};
