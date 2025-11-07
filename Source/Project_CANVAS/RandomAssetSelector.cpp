// Fill out your copyright notice in the Description page of Project Settings.


#include "RandomAssetSelector.h"

FString URandomAssetSelector::GetRandomAssetName(FString UserPrompt, TArray<FString> List)
{
	return List[0];
}

void URandomAssetSelector::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
}
