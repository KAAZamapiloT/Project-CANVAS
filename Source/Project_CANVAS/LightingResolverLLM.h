// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include"HttpModule.h"
#include "LightingResolverLLM.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLightingPlanReady,FString,PlanString);
UCLASS()
class PROJECT_CANVAS_API ULightingResolverLLM : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable)
	void RequestPlan(FString UserPrompt,UWorld* World,class USceneHistoryManager* HistoryManager);
	
	void OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	FString ConstructPlanPrompt(FString UserPrompt,class UAssetIndexer*Indexer);
	
    UPROPERTY(BlueprintAssignable)
	FOnLightingPlanReady OnLightingPlanReady;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
    {
        Super::Initialize(Collection);
    }
	virtual void Deinitialize() override
	{
		Super::Deinitialize();
	}

	

};
