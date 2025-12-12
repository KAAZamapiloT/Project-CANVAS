#include "RandomAssetSelector.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "AssetIndexer.h"
#include "Misc/Paths.h"

FString URandomAssetSelector::GetRandomAssetName(FString UserPrompt, TArray<FString> List)
{
	return List.Num() > 0 ? List[0] : TEXT("");
}

FString URandomAssetSelector::PruneMeshAssets(const FString& JsonPlan, UAssetIndexer* Indexer)
{
    if (!Indexer) return TEXT("");

    TSet<FString> ValidatedAssets; 
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPlan);

    if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
    {
        // 1. Process Static Meshes
        const TArray<TSharedPtr<FJsonValue>>* Spawns;
        if (JsonObj->TryGetArrayField(TEXT("SpawnRequest"), Spawns))
        {
            for (const auto& Val : *Spawns)
            {
                TSharedPtr<FJsonObject> Item = Val->AsObject();
                if (Item.IsValid())
                {
                    FString AssetConcept = Item->GetStringField(TEXT("AssetPath"));
                    if (!AssetConcept.IsEmpty()) 
                    {
                        // Smart Lookup: Meshes
                        TArray<FString> RealAssets = Indexer->GetTopKMeshesForQuery(AssetConcept, 5);
                        for (const FString& Path : RealAssets)
                        {
                            if (Path.StartsWith(TEXT("/Engine/")) || Path.Contains(TEXT("/Developers/"))) continue;
                            ValidatedAssets.Add(FPaths::GetBaseFilename(Path));
                        }
                    }
                }
            }
        }

        // 2. Process Particles (NEW ADDITION)
        const TArray<TSharedPtr<FJsonValue>>* Particles;
        if (JsonObj->TryGetArrayField(TEXT("ParticleSpawn"), Particles))
        {
            for (const auto& Val : *Particles)
            {
                TSharedPtr<FJsonObject> Item = Val->AsObject();
                if (Item.IsValid())
                {
                    FString AssetConcept = Item->GetStringField(TEXT("AssetPath"));
                    if (!AssetConcept.IsEmpty()) 
                    {
                        // Smart Lookup: Particles (Reuse Mesh query or add specific particle query in Indexer)
                        // Assuming GetTopKMeshesForQuery handles generic asset search or you add a specific Particle one
                        // For now, we search generic or assume exact match from the Smart List provided earlier
                         TArray<FString> RealFX = Indexer->GetTopKMeshesForQuery(AssetConcept, 3); // Or specialized function
                         for (const FString& Path : RealFX)
                         {
                             ValidatedAssets.Add(FPaths::GetBaseFilename(Path));
                         }
                    }
                }
            }
        }
    }
    
    return FString::Join(ValidatedAssets.Array(), TEXT("\", \""));
}

FString URandomAssetSelector::PruneTextureAssets(const FString& JsonPlan, UAssetIndexer* Indexer)
{
	if (!Indexer) return TEXT("");

	TSet<FString> ValidatedMaterials;
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPlan);

	if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Props;
		if (JsonObj->TryGetArrayField(TEXT("Props"), Props))
		{
			for (const auto& Val : *Props)
			{
				TSharedPtr<FJsonObject> Item = Val->AsObject();
				if (Item.IsValid())
				{
					const TSharedPtr<FJsonObject>* TexObj;
					if (Item->TryGetObjectField(TEXT("Texture"), TexObj))
					{
						// 1. Get Concept (e.g. "Concrete")
						FString MatConcept = (*TexObj)->GetStringField(TEXT("BaseColorPath"));
						
						if (!MatConcept.IsEmpty()) 
						{
							// 2. Smart Lookup
							TArray<FString> RealMats = Indexer->GetTopKTexturesForQuery(MatConcept, 5);
							
							for (const FString& Mat : RealMats)
							{
                                // Filter out utility textures
                                if (Mat.Contains(TEXT("Grid")) || Mat.Contains(TEXT("_Normal"))) continue;
								ValidatedMaterials.Add(Mat);
							}
						}
					}
				}
			}
		}
	}
	return FString::Join(ValidatedMaterials.Array(), TEXT("\", \""));
}