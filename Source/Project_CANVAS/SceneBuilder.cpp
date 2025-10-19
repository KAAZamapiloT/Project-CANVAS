// Fill out your copyright notice in the Description page of Project Settings.


#include "SceneBuilder.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
void USceneBuilder::BuildScene(const struct FEnhancedScenePlan& Plan,UWorld* WorldContext)
{
if (!WorldContext)
{
	UE_LOG(LogTemp,Error,TEXT("Scene Context is NULL"));
	return;
}
	// step 1 -> apply transformation to only walls
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsWithTag(WorldContext, FName("Background.Wall"), FoundActors);

	if (FoundActors.Num() == 0)
	{
		UE_LOG(LogTemp,Error,TEXT("Background.Wall not found"));
		return;
	}

	// step 2 -> find mesh of wall
	for (AActor* Actor : FoundActors)
	{
		UStaticMeshComponent* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>();

// step 3 if mesh is found
		if (Mesh)
		{
			// 2. Get its material and set the color parameter
			UMaterialInstanceDynamic* DynMaterial = Mesh->CreateAndSetMaterialInstanceDynamic(0);
			if (DynMaterial)
			{
				DynMaterial->SetVectorParameterValue(FName("BaseColor"), Plan.BackgroundColor);
				//UE_LOG(LogTemp,Display,TEXT("BaseColor: %s",Plan.BackgroundColor));
				
				if (!Plan.TextureName.IsEmpty())
				{
					FString PathString = "/Game/DATABASE/textures/" + Plan.TextureName + "." + Plan.TextureName;
					FSoftObjectPath AssetPath(PathString);
					UTexture2D* TextureToApply = Cast<UTexture2D>(AssetPath.TryLoad());
					UE_LOG(LogTemp, Warning, TEXT("BUILDER: Received TextureName: %s"), *Plan.TextureName);
					if (TextureToApply)
					{
						// SUCCESS! Apply the texture.
						DynMaterial->SetTextureParameterValue(FName("BaseTexture"), TextureToApply);
                    
						// (Optional) We set the color to white so it doesn't "tint" the texture.
						DynMaterial->SetVectorParameterValue(FName("BaseColor"), FColor::White);
						UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Successfully applied override texture %s"), *Plan.TextureName);
					}
					else
					{
						// FAILURE! Log an error. The color from Step 1 will remain.
						UE_LOG(LogTemp, Error, TEXT("SceneBuilder: FAILED to load texture: %s. Using fallback color."), *AssetPath.ToString());
					}
				}
			}
		}

		// step 4 applying texture

		
		
	
	}

	
}
