#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "PaintingResolverLLM.generated.h"

// Broadcasts the JSON plan (to be parsed by GenAISystem/JsonParser) and a summary string
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPaintingPlanReady, FString, PaintingJson, FString, Summary);

/**
 * The "Director" Agent.
 * USES: Groq (Llama 3.3 70B)
 * INPUT: User Prompt + Canvas Bounds + Semantic Asset Tags
 * OUTPUT: JSON list of "PaintingCommands" (Tools, Styles, Zones)
 */
UCLASS()
class PROJECT_CANVAS_API UPaintingResolverLLM : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Main entry point
	UFUNCTION(BlueprintCallable, Category = "GenAI")
	void RequestPaintingPlan(FString UserPrompt, UWorld* World, class UAssetIndexer* Indexer);

private:
	// 1. Prompt Engineering
	FString CreateDirectorPayload(FString UserPrompt, class UAssetIndexer* Indexer, FString BoundsContext);

	// 2. HTTP Callback
	void OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

public:
	UPROPERTY(BlueprintAssignable)
	FOnPaintingPlanReady OnPaintingPlanReady;
};