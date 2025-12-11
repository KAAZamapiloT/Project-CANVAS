// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include"HttpModule.h"
#include "MeshResolverLLM.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMeshPlanReady,FString ,PlanJson,FString,Choices);

UCLASS()
class PROJECT_CANVAS_API UMeshResolverLLM : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	FString CreateMeshPayload(FString UserPrompt,class UAssetIndexer*Indexer);
	
	UFUNCTION(BlueprintCallable)
	void RequestPlan(FString UserPrompt,UWorld* World,class USceneHistoryManager* HistoryManager);
	
	void OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	UPROPERTY(BlueprintAssignable)
	FOnMeshPlanReady OnMeshPlanReady;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);
	}
	virtual void Deinitialize() override
	{
		Super::Deinitialize();
	}
};
