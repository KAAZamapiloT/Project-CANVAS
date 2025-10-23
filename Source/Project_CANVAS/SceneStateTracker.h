// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include"AssetIndexer.h"
#include "SceneStateTracker.generated.h"
class UGenAISystem;
class USceneBuilder;
class USceneHistoryManager;
/**
 * 
 */
UCLASS()
class PROJECT_CANVAS_API USceneStateTracker : public UGameInstance
{
	GENERATED_BODY()
public:
	UPROPERTY()
	UAssetIndexer* AssetIndexer;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Data")
	TArray<FString> TargetableActorTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Data")
	TArray<FString> TargetablePostProcessMaterials;
	virtual void Init() override;
	// ADD THESE TWO:
	UPROPERTY()
	UGenAISystem* GenAISystem;
    
	UPROPERTY()
	USceneBuilder* SceneBuilder;
	UPROPERTY()
	USceneHistoryManager* HistoryManager;
	
	UFUNCTION() // Must be a UFUNCTION to bind to a delegate
	void OnAssetScanFinished();
	UFUNCTION()
	void OnPlanReceived(const FEnhancedScenePlan& Plan,const FString& UserPrompt);
	UFUNCTION()
	void ResolveTexturesFromNames(FEnhancedScenePlan& Plan);
};
