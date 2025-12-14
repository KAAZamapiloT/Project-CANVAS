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

	
	UFUNCTION(BlueprintCallable)
	void RequestPlan(FString UserPrompt,UWorld* World,class USceneHistoryManager* HistoryManager);
	
	void OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// New Pruned Request Function
	UFUNCTION(BlueprintCallable)
	void RequestPlan_Pruned(FString UserPrompt, FString& PrunedAssets, UWorld* World, class USceneHistoryManager* HistoryManager);
	
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
private:
	FString CreateMeshPayload(FString UserPrompt,class UAssetIndexer*Indexer);
	// Helper to generate prompt from specific list
	FString CreatePrunedMeshPayload(FString UserPrompt, FString& PrunedAssets);
};
