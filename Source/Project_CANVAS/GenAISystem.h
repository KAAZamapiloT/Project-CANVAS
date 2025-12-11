// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h" 
#include "HttpModule.h"
#include "API_KEY.h"
#include "GenAISystem.generated.h"
class USceneHistoryManager;
/**
 * 
 */
//  A EVENT DRIVEN -> SEND IT TO SCENE BUILDER
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThemeDataReady, 
	const FEnhancedScenePlan&, Plan,
	const FString&, UserPrompt); 
UCLASS()
class PROJECT_CANVAS_API UGenAISystem : public UObject
{
	GENERATED_BODY()
public:
	UGenAISystem();
	/**
 * Construct comprehensive LLM prompt with available assets from AssetIndexer
 * Gets all materials, meshes, tags, and post-process materials directly
 * 
 * @param UserPrompt - User's request
 * @param AssetIndexer - Populated asset database (already scanned)
 * @return Formatted prompt for LLM
 */
	FString ConstructMasterPrompt(
		FString UserPrompt,
		class UAssetIndexer* AssetIndexer
	);

	UFUNCTION(BlueprintCallable)
	void RequestSceneChange(FString UserPrompt,UWorld* WorldContext,USceneHistoryManager* HistoryManager);
	
	// --- Internal HTTP Callback ---
	void OnLLMResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	
	// We need a reference to our parser
	UPROPERTY(BlueprintAssignable)
	FOnThemeDataReady OnThemeDataReady;

private:
	FString LastUserPrompt ;

	UFUNCTION()
	void OnTexturePlanReady(FString TexturePlan, TArray<FString> ActorTags);

	UFUNCTION()
	void OnMeshPlanReady(FString Plan,FString Choices);

	UFUNCTION()
	void OnLightingPlanReady(FString Plan);
};
