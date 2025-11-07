// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include"HttpModule.h"
#include "RandomAssetSelector.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_CANVAS_API URandomAssetSelector : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION()
	FString GetRandomAssetName(FString UserPrompt,TArray<FString> List);
private:
	void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
};
