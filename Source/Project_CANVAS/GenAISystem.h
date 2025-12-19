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

void Initialize();

void Deinitialize();	
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

	FString ConstructPrunedMasterPrompt(FString UserPrompt,
		class UAssetIndexer* AssetIndexer);
	UFUNCTION(BlueprintCallable)
	void RequestSceneChange(FString UserPrompt,UWorld* WorldContext,USceneHistoryManager* HistoryManager);
	
	// --- Internal HTTP Callback ---
	void OnLLMResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	void AttemptSynthesis(FString UserPrompt,UWorld*WorldContext,USceneHistoryManager*HistoryManager);

	
	// We need a reference to our parser
	UPROPERTY(BlueprintAssignable)
	FOnThemeDataReady OnThemeDataReady;

	
	


	void ExecuteFallbackPlan();
private:
	UPROPERTY()
	class UMeshResolverLLM* MeshL ;
	
	UPROPERTY()
	class UTextureResolverLLM* TexL ;

	UPROPERTY()
	class UIntentionResolverLLM* IntentionL;

	UPROPERTY()
	class UPaintingResolverLLM* PaintL;
	// Intermediate Data Storage (The "Puzzle Pieces")
	FString DraftMeshJson;
	
	FString DraftTexJson;


	FString PrunedMeshList;      // From MeshResolver Choices
	FString PrunedTextureList;   // From TextureResolver Choices
	
	bool bIsMeshReady=false;
	bool bIsTexReady=false;
	FString LastUserPrompt ;

	TWeakObjectPtr<UWorld> CachedWorld;
	UPROPERTY()
	USceneHistoryManager* CachedHistory;
	UFUNCTION()
	void OnTexturePlanReady(FString TexturePlan, FString Choices);

	UFUNCTION()
	void OnMeshPlanReady(FString Plan,FString Choices);

	UFUNCTION()
	void OnIntentionReady(const TArray<FString>& MeshKeywords,
		const TArray<FString>& TextureKeywords, const TArray<FString>& ParticleKeywords);

	UFUNCTION()
	void OnPaintingPlanReady(FString Plan,FString Sum);
	TArray<FString> PrunedMeshesContext;
	TArray<FString> PrunedTexturesContext;
	TArray<FString> PrunedParticlesContext;
	bool bHasSynthesized = false;
	// NEW: Store the draft plan from the Painting Agent
	FString DraftPaintingJson; 
	
	/**
	 * Applies a lighting preset based on keywords in the prompt, 
	 * or picks a random valid archetype if no keywords match.
	 */
	void ApplySmartFallbackLighting(struct FEnhancedScenePlan& Plan, const FString& Prompt);
	// NEW: Flag to track if Painting Agent has finished
	UPROPERTY(EditAnywhere)
	bool bIsPaintingReady = false;
	UPROPERTY(EditAnywhere)
	bool bPaintL=true;
	UPROPERTY(EditAnywhere)
	bool bIntent=true;

	UPROPERTY(EditAnywhere)
	bool bIsIntentReady=false;
	
	/*
	 * A way to bypass mutistage sytem and go to a legacy one with minor optmizations
	 */
	UPROPERTY(EditAnywhere)
	bool bSpeedMode=false;
public:

FORCEINLINE void SetSpeedMode(bool SpeedMode){ bSpeedMode=SpeedMode; }

FORCEINLINE bool GetSpeedMode() const{ return bSpeedMode;}
	
};
