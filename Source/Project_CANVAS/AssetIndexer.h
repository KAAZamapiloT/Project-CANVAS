// AssetIndexer.h - FINAL CLEAN VERSION

#pragma once

/** @file AssetIndexer.h
 * @brief Contains the UAssetIndexer class, responsible for discovering, caching, and resolving game assets.
 */

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h"
#include "AssetIndexer.generated.h"

// ========================================
// STRUCTS
// ========================================

/**
 * @struct FMeshAssetInfo
 * @brief Holds detailed information for a single discovered mesh asset, including its path and searchable keywords.
 */
USTRUCT(BlueprintType)
struct FMeshAssetInfo
{
    GENERATED_BODY()
    
    /// @brief The simple name of the mesh (e.g., "SM_Chair").
    UPROPERTY()
    FString MeshName;
    
    /// @brief The full asset registry path (e.g., "/Game/DATABASE/meshes/SM_Chair.SM_Chair").
    UPROPERTY()
    FString FullPath;
    
    /// @brief The directory containing the mesh (e.g., "/Game/DATABASE/meshes").
    UPROPERTY()
    FString Directory;
    
    /// @brief Searchable keywords extracted from the name and path.
    UPROPERTY()
    TArray<FString> Keywords;
};

/**
 * @struct FMeshVariantGroup
 * @brief Represents a collection of mesh variants for a single base name (e.g., "Chair" -> "Chair_01", "Chair_02").
 */
USTRUCT(BlueprintType)
struct FMeshVariantGroup
{
    GENERATED_BODY()
    
    /// @brief The base name for this group (e.g., "Chair").
    UPROPERTY()
    FString BaseName;
    
    /// @brief Array of simple variant names (e.g., "Chair_01", "Chair_02").
    UPROPERTY()
    TArray<FString> Variants;
    
    /// @brief Array of full asset paths for each variant.
    UPROPERTY()
    TArray<FString> VariantPaths;
    
    /**
     * @brief Gets a random simple name from the variant list.
     * @return A random variant name, or an empty string if no variants exist.
     */
    FString GetRandomVariant() const
    {
        if (Variants.Num() == 0) return TEXT("");
        return Variants[FMath::RandRange(0, Variants.Num() - 1)];
    }
    
    /**
     * @brief Gets a random full asset path from the variant list.
     * @return A random variant asset path, or an empty string if no variants exist.
     */
    FString GetRandomVariantPath() const
    {
        if (VariantPaths.Num() == 0) return TEXT("");
        return VariantPaths[FMath::RandRange(0, VariantPaths.Num() - 1)];
    }
};

// ========================================
// DELEGATES
// ========================================

/**
 * @brief Delegate broadcast when the asynchronous asset scan has fully completed.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAssetScanComplete);

// ========================================
// CLASS DECLARATION
// ========================================

/**
 * @class UAssetIndexer
 * @brief Manages comprehensive asset discovery, caching, and resolution for the project.
 *
 * Responsibilities:
 * - Asynchronously scan all asset types (meshes, textures, particles, post-process) from specific database paths.
 * - Build searchable databases for resolving asset names.
 * - Provide robust asset name resolution (fuzzy matching, variant groups, keyword search).
 * - Support random variant selection for meshes.
 */
UCLASS()
class PROJECT_CANVAS_API UAssetIndexer : public UObject
{
    GENERATED_BODY()

public:
    
    // ========================================
    // SCANNING FUNCTIONS
    // ========================================
    
    /**
     * @brief Initiates an asynchronous scan for all supported asset types.
     * @param WorldContext A valid UWorld context.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanAllAssetsAsync(UWorld* WorldContext);
    
    /**
     * @brief Initiates an asynchronous scan for textures.
     * @param ScanPath The directory path to scan (defaults to "/Game/DATABASE/textures").
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanForTexturesAsync(FString ScanPath = TEXT("/Game/DATABASE/textures"));
    
    /**
     * @brief Initiates an asynchronous scan for particle systems.
     * @param ScanPath The directory path to scan (defaults to "/Game/DATABASE/particles").
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanForParticlesAsync(FString ScanPath = TEXT("/Game/DATABASE/particles"));
    
    /**
     * @brief Initiates an asynchronous scan for post-process materials.
     * @param ScanPath The directory path to scan (defaults to "/Game/DATABASE/postprocess").
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanForPostProcessMaterialsAsync(FString ScanPath = TEXT("/Game/DATABASE/postprocess"));
    
    /**
     * @brief Initiates an asynchronous scan for static meshes and builds variant groups.
     * @param ScanPath The directory path to scan (defaults to "/Game/DATABASE/meshes").
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanForStaticMeshesAsync(FString ScanPath = TEXT("/Game/DATABASE/meshes"));
    
    /**
     * @brief Scans the current level for all unique actor tags.
     * @param WorldContext A valid UWorld context.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Scan")
    void ScanActorTagsInLevel(UWorld* WorldContext);
    
    // ========================================
    // TEXTURE QUERIES
    // ========================================
    
    /**
     * @brief Gets the list of all discovered texture names.
     * @return TArray of texture names.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    TArray<FString> GetDiscoveredTextureNames() const { return DiscoveredTextureNames; }
    
    /**
     * @brief Resolves a search name to a full FTextureSet.
     * @param SearchName The name to search for (e.g., "Concrete").
     * @return The matching FTextureSet, which may be empty if not found.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    FTextureSet ResolveTextureFromName(const FString& SearchName);
    
    /**
     * @brief Resolves a base material name (e.g., "MI_Concrete") to its corresponding FTextureSet.
     * @param BaseMaterialName The name of the material.
     * @return The matching FTextureSet.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    FTextureSet ResolveBaseMaterialToTextureSet(const FString& BaseMaterialName);
    
    /**
     * @brief Gets the names of all base materials found.
     * @return TArray of base material names.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    TArray<FString> GetMaterialBaseNames() const
    {
        TArray<FString> Names;
        MaterialDatabase.GetKeys(Names);
        return Names;
    }
    
    /**
     * @brief Builds and returns the complete material-to-texture database.
     * @return A TMap where the key is the material name and the value is its FTextureSet.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Textures")
    TMap<FString, FTextureSet> BuildMaterialDatabase();
    
    // ========================================
    // PARTICLE QUERIES
    // ========================================
    
    /**
     * @brief Gets the list of all discovered particle system names.
     * @return TArray of particle names.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Particles")
    TArray<FString> GetDiscoveredParticleNames() const { return DiscoveredParticleNames; }
    
    // ========================================
    // POST-PROCESS QUERIES
    // ========================================
    
    /**
     * @brief Gets the list of all discovered post-process material names.
     * @return TArray of post-process material names.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|PostProcess")
    TArray<FString> GetDiscoveredPostProcessNames() const { return DiscoveredPostProcessNames; }
    
    // ========================================
    // ACTOR TAG QUERIES
    // ========================================
    
    /**
     * @brief Gets the list of all unique actor tags discovered in the level.
     * @return TArray of actor tag strings.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Tags")
    TArray<FString> GetDiscoveredActorTags() const { return DiscoveredActorTags; }
    
    // ========================================
    // MESH RESOLUTION (NEW - PRIMARY)
    // ========================================
    
    /**
     * @brief Resolves a mesh search name to a full asset path, with support for variants.
     *
     * Strategies:
     * 1. Exact numbered variant (e.g., "Chair_02") -> returns "Chair_02" path.
     * 2. Variant group (e.g., "Chair") -> returns a *random* variant path ("Chair_01", "Chair_02", etc.).
     * 3. Fallback to robust single-mesh resolution.
     *
     * @param SearchName The name to search for (e.g., "Chair" or "Chair_02").
     * @return A full asset path (e.g., "/Game/DATABASE/meshes/SM_Chair_02.SM_Chair_02"), or empty string if not found.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    FString ResolveMeshToFullPathWithVariants(const FString& SearchName);
    
    /**
     * @brief Resolves a mesh search name to a single, best-match full asset path (no variant logic).
     *
     * Strategies:
     * 1. Exact normalized match.
     * 2. Substring match.
     * 3. Keyword semantic match.
     * 4. Fuzzy string match (60%+ similarity).
     * 5. Fallback to a random SM_ prefixed mesh if no other match is found.
     *
     * @param SearchName The name to search for.
     * @return A single full asset path, or an empty string if not found.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    FString ResolveMeshToFullPath(const FString& SearchName);
    
    // ========================================
    // MESH VARIANT QUERIES
    // ========================================
    
    /**
     * @brief Gets the variant group for a given mesh base name.
     * @param SearchName The base name (e.g., "Chair").
     * @return A FMeshVariantGroup struct containing all variants for that base name.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    FMeshVariantGroup GetMeshVariants(const FString& SearchName);
    
    /**
     * @brief Counts the number of variants for a given mesh base name.
     * @param SearchName The base name (e.g., "Chair").
     * @return The number of variants (e.g., 3).
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    int32 GetVariantCount(const FString& SearchName);
    
    // ========================================
    // MESH INFO QUERIES
    // ========================================
    
    /**
     * @brief Gets detailed asset information for a specific mesh name.
     * @param SearchName The exact name of the mesh (e.g., "SM_Chair_01").
     * @return A FMeshAssetInfo struct.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    FMeshAssetInfo GetMeshInfo(const FString& SearchName);
    
    /**
     * @brief Gets the names of all discovered individual mesh assets.
     * @return TArray of all mesh names (e.g., "SM_Chair_01", "SM_Table_01").
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    TArray<FString> GetAllMeshNames() const;

    /**
     * @brief Resolves a search name to *all* possible matching mesh paths.
     * If a base name is given, returns all variant paths. If a specific name is given, returns that path.
     * @param SearchName The base name (e.g., "Chair") or specific name.
     * @return TArray of all matching full asset paths.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    TArray<FString> ResolveAllMeshPaths(const FString& SearchName);

    /**
     * @brief Gets the total count of all discovered individual mesh assets.
     * @return The number of meshes in the DiscoveredMeshes map.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Meshes")
    int32 GetMeshCount() const { return DiscoveredMeshes.Num(); }
    
    // ========================================
    // DEBUG & LOGGING
    // ========================================
    
    /**
     * @brief Prints the complete discovered mesh database to the output log.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Debug")
    void PrintAllMeshes() const;
    
    /**
     * @brief Prints all discovered mesh variant groups to the output log.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer|Debug")
    void PrintMeshVariants() const;
    
    // ========================================
    // STATE
    // ========================================
    
    /**
     * @brief Checks if the initial asynchronous scan is complete.
     * @return true if all scans are finished, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
    bool IsScanComplete() const { return bIsScanComplete; }
    
    /// @brief Delegate broadcast when all async scans are complete.
    UPROPERTY(BlueprintAssignable, Category = "AssetIndexer")
    FOnAssetScanComplete OnScanComplete;

private:
    
    // ========================================
    // MESH STORAGE (NEW)
    // ========================================
    
    /// @brief Main database mapping a normalized mesh name to its asset info.
    UPROPERTY()
    TMap<FString, FMeshAssetInfo> DiscoveredMeshes;
    
    /// @brief Database mapping a base name (e.g., "chair") to its group of variants.
    UPROPERTY()
    TMap<FString, FMeshVariantGroup> VariantGroups;
    
    /// @brief Cached list of all mesh names for quick lookups.
    UPROPERTY()
    TArray<FString> MeshNameCache;
   
    
    // ========================================
    // OTHER ASSET STORAGE (EXISTING)
    // ========================================
    
    /// @brief Cached list of discovered texture names.
    UPROPERTY()
    TArray<FString> DiscoveredTextureNames;
    
    /// @brief Cached list of discovered particle names.
    UPROPERTY()
    TArray<FString> DiscoveredParticleNames;
    
    /// @brief Cached list of discovered post-process material names.
    UPROPERTY()
    TArray<FString> DiscoveredPostProcessNames;
    
    /// @brief Cached list of discovered actor tags from the level.
    UPROPERTY()
    TArray<FString> DiscoveredActorTags;
    
    /// @brief Legacy list of static mesh names (superseded by DiscoveredMeshes).
    UPROPERTY()
    TArray<FString> DiscoveredStaticMeshNames;
    
    /// @brief Database mapping a base material name to its FTextureSet.
    UPROPERTY()
    TMap<FString, FTextureSet> MaterialDatabase;
    
    // ========================================
    // STATE
    // ========================================
    
    /// @brief Flag to prevent concurrent scans.
    UPROPERTY()
    bool bIsScanning = false;
    
    /// @brief Flag indicating if all initial scans are complete.
    UPROPERTY()
    bool bIsScanComplete = false;
    
    /// @brief Counter for tracking pending asynchronous scan tasks.
    int32 PendingScans = 0;
    
    // ========================================
    // INTERNAL HELPERS
    // ========================================
    
    /**
     * @brief Core scanning logic using AssetRegistry.
     * @param AssetClass The UClass to scan for.
     * @param ScanPath The directory to scan.
     * @param OutArray The TArray to populate with results.
     */
    void ScanAssetsOfType(const UClass* AssetClass, FString ScanPath, TArray<FString>& OutArray);
    
    /// @brief Called after each async scan finishes to check if all are complete.
    void CheckAllScansComplete();
    
    /**
     * @brief Extracts the base name from a variant name (e.g., "SM_Chair_01" -> "chair").
     * @param VariantName The full name of the variant asset.
     * @return The normalized base name.
     */
    FString ExtractBaseName(const FString& VariantName);
    
    /**
     * @brief Checks if a name is a numbered variant (e.g., "Name_01", "Name_02").
     * @param Name The name to check.
     * @param OutVariantNumber The variant number if found.
     * @return true if it is a numbered variant.
     */
    bool IsNumberedVariant(const FString& Name, int32& OutVariantNumber);
    
    /**
     * @brief Normalizes a name for matching (lowercase, no prefixes).
     * @param Name The name to normalize.
     * @return The normalized string.
     */
    FString NormalizeName(const FString& Name);
    
    /**
     * @brief Extracts keywords from a mesh name for searching.
     * @param MeshName The name of the mesh.
     * @return TArray of keywords.
     */
    TArray<FString> ExtractKeywordsFromMesh(const FString& MeshName);
    
    /**
     * @brief Calculates Levenshtein distance similarity between two strings.
     * @param A The first string.
     * @param B The second string.
     * @return Similarity score.
     */
    int32 CalculateSimilarity(const FString& A, const FString& B);
    
    /**
     * @brief Extracts a base name from a texture name (e.g., "T_Concrete_BC" -> "Concrete").
     * @param TextureName The full name of the texture asset.
     * @return The extracted base name.
     */
    FString ExtractMaterialBaseName(const FString& TextureName);
};