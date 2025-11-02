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
	static bool bScehmaValidation(FString& JsonContext);
	UFUNCTION(BlueprintCallable)
	static FEnhancedScenePlan CreatePlan(FString JsonContext);

private:
	static void ParseEnvironment(const TSharedPtr<FJsonObject>& JsonObject, FEnvironmentPlan& Environment);
	static void ParsePropModification(const TSharedPtr<FJsonObject>& JsonObject, FPropsModification& PropMod);
	static void ParseTextureSet(const TSharedPtr<FJsonObject>& JsonObject, FTextureSet& TextureSet);
	static void ParseSpawnRequest(const TSharedPtr<FJsonObject>& JsonObject, FSpawnRequest& SpawnRequest)
};
