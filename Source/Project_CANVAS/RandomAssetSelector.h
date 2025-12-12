#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RandomAssetSelector.generated.h"
class UAssetIndexer;
UCLASS()
class PROJECT_CANVAS_API URandomAssetSelector : public UObject
{
	GENERATED_BODY()
public:
	// Existing function
	UFUNCTION()
	FString GetRandomAssetName(FString UserPrompt, TArray<FString> List);
	

	/** * @brief Extracts mesh concepts from the draft JSON and resolves them to real assets.
	 * * @param JsonPlan The raw JSON output from the MeshResolverLLM.
	 * @param Indexer The AssetIndexer to use for looking up groups/variants.
	 * @return A comma-separated string of VALID asset names/paths (e.g., "Chair_01", "Chair_02", "Table_A").
	 */
	static FString PruneMeshAssets(const FString& JsonPlan, UAssetIndexer* Indexer);

	/** * @brief Extracts texture concepts from the draft JSON and resolves them to real materials.
	 * * @param JsonPlan The raw JSON output from the TextureResolverLLM.
	 * @param Indexer The AssetIndexer to use for lookup.
	 * @return A comma-separated string of VALID material names.
	 */
	static FString PruneTextureAssets(const FString& JsonPlan, UAssetIndexer* Indexer);
};