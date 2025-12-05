// LocationResolverLLM.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include"ScenePlan.h"
#include "API_KEY.h"
#include "LocationResolverLLM.generated.h"

struct FSpawnRequest;
// ✅ NEW STRUCT: Holds Location, Rotation, AND Scale
USTRUCT(BlueprintType)
struct FResolutionResult
{
    GENERATED_BODY()

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    float RotationYaw = 0.0f;

    UPROPERTY()
    float Scale = 1.0f;
};

// Update the typedef to use the struct
typedef TMap<FString, FResolutionResult> FLocationMap;
DECLARE_DELEGATE_OneParam(FOnBatchLocationsResolved, const FLocationMap&);

/**
 * Minimal LLM location resolver.
 * Supports remote (cloud) and local (LM Studio/Ollama) models.
 */
UCLASS()
class PROJECT_CANVAS_API ULocationResolverLLM : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Configure resolver.
     * @param Endpoint - API URL (e.g., "http://localhost:1234/v1/chat/completions")
     * @param APIKey - API key (empty = local model, no auth)
     * @param ModelName - Model name
     */
    UFUNCTION(BlueprintCallable, Category = "LocationResolver")
    void Configure(const FString& Endpoint, const FString& APIKey, const FString& ModelName);

    /**
     * Main resolution function.
     */
 //   UFUNCTION(BlueprintCallable, Category = "LocationResolver")
    // CHANGE THIS LINE:
    FTransform ResolveLocation(
        const FString& LocationName,
        const FString& SceneContext,
        const FSpawnRequest* SpawnRequest = nullptr 
    );

    void ResolveBatchLocationsAsync(
         const TArray<FSpawnRequest>& Requests,
         const FString& SceneContext,
         FOnBatchLocationsResolved Callback
     );
    FString BuildBatchPrompt(const TArray<FSpawnRequest>& Requests, const FString& SceneContext);

    UFUNCTION(BlueprintPure, Category = "LocationResolver")
    bool IsEnabled() const { return !Endpoint.IsEmpty(); }

    UFUNCTION(BlueprintCallable, Category = "LocationResolver")
    void ClearCache() { Cache.Empty(); }

private:
    /** Makes request to remote API (with auth) */
    FString ResolveUsingRemote(const FString& Prompt);

    /** Makes request to local API (no auth) */
    FString ResolveUsingLocal(const FString& Prompt);

    /** Build prompt */
    FString BuildPrompt(const FString& LocationName, const FString& SceneContext, const FSpawnRequest* SpawnRequest);

    /** Parse JSON response */
    bool ParseTransform(const FString& JsonResponse, FTransform& OutTransform);

    // Config
    FString Endpoint;
    FString APIKey;
    FString ModelName;
    
    // Cache
    TMap<FString, FTransform> Cache;
    
    float Timeout = 2.0f;
};
