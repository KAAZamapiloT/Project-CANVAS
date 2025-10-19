// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h" 
#include "HttpModule.h"
#include "GenAISystem.generated.h"

/**
 * 
 */
//  A EVENT DRIVEN -> SEND IT TO SCENE BUILDER
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThemeDataReady, const FEnhancedScenePlan&, Plan);
UCLASS()
class PROJECT_CANVAS_API UGenAISystem : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable)
	FOnThemeDataReady OnThemeDataReady;

	UFUNCTION(BlueprintCallable)
	void RequestSceneChange(FString UserPrompt);
	
	// --- Internal HTTP Callback ---
	void OnOllamaResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	// We need a reference to our parser
	UPROPERTY()
	class UJsonParser* Parser;

	// Helper function to build the final prompt for the LLM
	FString ConstructMasterPrompt(FString UserPrompt);
};
