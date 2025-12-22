// LocationResolverLLM.h

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "ScenePlan.h"
#include "API_KEY.h"
#include "LocationResolverLLM.generated.h"

struct FSpawnRequest;

// Standardized Result Struct
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

    // ✅ NEW: Layout Pattern Support
    // 0 = Single, 1 = Circle, 2 = Grid, 3 = Scatter
    UPROPERTY()
    int32 PatternID = 0;

    UPROPERTY()
    float PatternRadius = 0.0f;
};

typedef TMap<FString, FResolutionResult> FLocationMap;
DECLARE_DELEGATE_OneParam(FOnBatchLocationsResolved, const FLocationMap&);

/**
 * Robust Location Resolver supporting Gemini and Groq/OpenAI automatically.
 */
UCLASS()
class PROJECT_CANVAS_API ULocationResolverLLM : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Configure resolver.
     * To use Gemini: Endpoint = "https://generativelanguage.googleapis.com...", APIKey = "" (Key in URL)
     * To use Groq:   Endpoint = "https://api.groq.com...", APIKey = "gsk_..."
     */
    UFUNCTION(BlueprintCallable, Category = "LocationResolver")
    void Configure(const FString& Endpoint, const FString& APIKey, const FString& ModelName);

    // Single Location Resolve (Blocking/Synchronous logic wrapper)
    FTransform ResolveLocation(
        const FString& LocationName,
        const FString& SceneContext,
        const FSpawnRequest* SpawnRequest = nullptr 
    );

    // Batch Resolve (Async)
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
    // --- Internal Request Logic ---
    FString ResolveUsingRemote(const FString& Prompt);
    FString ResolveUsingLocal(const FString& Prompt);

    // --- Prompts ---
    FString BuildPrompt(const FString& LocationName, const FString& SceneContext, const FSpawnRequest* SpawnRequest);

    // --- Parsing Helpers ---
    bool ParseTransform(const FString& JsonResponse, FTransform& OutTransform);
    
    // Dynamic Parser Switchers
    FString ParseGeminiResponse(const FString& JsonResponse);
    FString ParseOpenAIResponse(const FString& JsonResponse);

    // Config
    FString Endpoint;
    FString APIKey;
    FString ModelName;
    
    // Cache
    TMap<FString, FTransform> Cache;
    float Timeout = 2.0f;
};