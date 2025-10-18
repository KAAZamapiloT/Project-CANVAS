// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include"ScenePlan.h"
#include "JsonParser.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_CANVAS_API UJsonParser : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	bool bScehmaValidation(FString& JsonContext);
	UFUNCTION(BlueprintCallable)
	FEnhancedScenePlan CreatePlan(FString JsonContext);
};
