// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include"HttpModule.h"
#include "LightingResolverLLM.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanReady,FString,PlanString);
UCLASS()
class PROJECT_CANVAS_API ULightingResolverLLM : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void RequestPlan(FString UserPrompt,UWorld* World,class USceneHistoryManager* HistoryManager);
	
	void OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	FString ConstructPlanPrompt(FString UserPrompt);
	
    UPROPERTY(BlueprintAssignable)
	FOnPlanReady OnPlanReady;
};
