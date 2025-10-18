// Fill out your copyright notice in the Description page of Project Settings.


#include "SceneBuilder.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "Components/SkyLightComponent.h"
#include "Components/DirectionalLightComponent.h"
void USceneBuilder::BuildScene(struct FEnhancedScenePlan& Plan,UWorld* WorldContext)
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
				
			}
		}
		
	}

	
}
