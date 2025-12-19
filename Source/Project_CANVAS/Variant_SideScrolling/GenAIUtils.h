#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GenAIUtils.generated.h"

/**
 * Static Utility library for GenAI JSON cleaning and parsing.
 * Can be called from anywhere: UGenAIUtils::FunctionName()
 */
UCLASS()
class PROJECT_CANVAS_API UGenAIUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 1. The Sanitizer (Strips Markdown, Comments, etc.)
	UFUNCTION(BlueprintCallable, Category = "GenAI|Utils")
	static FString CleanLLMResponse(FString RawResponse);

	// 2. A Helper to safely parse JSON String to Object (Reduces boilerplate)
	static TSharedPtr<FJsonObject> StringToJsonObject(FString JsonString);
};