// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h" 
#include "HttpModule.h"
#include "SceneStateTracker.h"
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
	
	// Helper function to build the final prompt for the LLM
	FString ConstructMasterPrompt(FString UserPrompt,const TArray<FString>& AvailableTextures,const TArray<FString>& AvailableTags,
		const TArray<FString>& AvailablePPMs,USceneHistoryManager* HistoryManager = nullptr);
	
	UFUNCTION(BlueprintCallable)
	void RequestSceneChange(FString UserPrompt,UWorld* WorldContext,USceneHistoryManager* HistoryManager);
	
	// --- Internal HTTP Callback ---
	void OnGroqResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	
	// We need a reference to our parser
	UPROPERTY(BlueprintAssignable)
	FOnThemeDataReady OnThemeDataReady;

private:
	FString LastUserPrompt ;
	FString ExtractMaterialBaseName(const FString& TextureName); 
};
