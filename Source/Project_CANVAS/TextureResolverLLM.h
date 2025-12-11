// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include"HttpModule.h"
#include "TextureResolverLLM.generated.h"

/**
 * SO MAIN FUCNTION OF THIS IS TO SELECT PROPS FOR MODIFICATIONS BASED ON USERS PLAN IT WILL HAVE FULL TEXTURES CONTEXT
 * IT WILL CHOSE AND PLAN AND BRODCAST IT I AM ALSO THINK OF BRODCASTING ALLL CANDIDATES TEXTURES FOR FINALPLAN REFINEMENT 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTextureResolution,TArray<FString>,TexturePlan,TArray<FString>,ActorTags);
UCLASS()
class PROJECT_CANVAS_API UTextureResolverLLM : public UObject
{
	GENERATED_BODY()
public:
	FString CreateMasterPrompt(FString UserPrompt,class UAssetIndexer* AssetIndexer);

	void OnResponseReceived(FHttpRequestPtr Request,FHttpResponsePtr Response,bool bWasSucessfull);

	UFUNCTION(BlueprintCallable)
	void RequestPlan(FString UserPrompt,UWorld*World,class USceneHistoryManager*HistoryManager);

	UPROPERTY(BlueprintAssignable)
    FOnTextureResolution OnTextureResolution;	
};
