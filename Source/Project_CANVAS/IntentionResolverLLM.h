#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HttpModule.h"
#include "IntentionResolverLLM.generated.h"

// Callback: Returns the LIST of keywords the LLM selected
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnIntentionReady, const TArray<FString>&, RelevantMeshes,
const TArray<FString>&, RelevantTexture,
const TArray<FString>&,RelavantParticles
);

UCLASS()
class PROJECT_CANVAS_API UIntentionResolverLLM : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // Ask the LLM to filter the massive list of categories down to what matters
    void RequestIntention(FString UserPrompt, const TArray<FString>& AllMeshes,
        const TArray<FString>& AllTextures,
const TArray<FString> AllParticles);

    UPROPERTY(BlueprintAssignable)
    FOnIntentionReady OnIntentionReady;

private:
    void OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
};