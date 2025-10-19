// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h"
#include "SceneBuilder.generated.h"

/**
 * 
 */


// TO DO :  search the components required -> Find Functanailty ->extract transform -> spawn / modify and apply -> sperate function for modfifcationa dna spawns and delettion (maybe)
UCLASS()
class PROJECT_CANVAS_API USceneBuilder : public UObject
{
	GENERATED_BODY()
	public:
	UFUNCTION(BlueprintCallable)
	void BuildScene(const struct FEnhancedScenePlan& Plan,UWorld* WorldContext);
};
