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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTexturePlanReady,FString,TexturePlan,FString,ActorTags);
UCLASS()
class PROJECT_CANVAS_API UTextureResolverLLM : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	

	void OnResponseReceived(FHttpRequestPtr Request,FHttpResponsePtr Response,bool bWasSucessfull);

	UFUNCTION(BlueprintCallable)
	void RequestPlan(FString UserPrompt,UWorld*World,class USceneHistoryManager*HistoryManager);

	// New Pruned Request Function
	UFUNCTION(BlueprintCallable)
	void RequestPlan_Pruned(FString UserPrompt, FString& PrunedAssets, UWorld* World, class USceneHistoryManager* HistoryManager);
	UPROPERTY(BlueprintAssignable)
   FOnTexturePlanReady OnTexturePlanReady;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override
	{
		Super::Initialize(Collection);
	}
	virtual void Deinitialize() override
	{
		Super::Deinitialize();
	}
private:
	FString CreateMasterPrompt(FString UserPrompt,class UAssetIndexer* AssetIndexer);
	FString CreatePrunedTexturePayload(FString UserPrompt,FString& PrunedAssets, UAssetIndexer* Indexer);
};
