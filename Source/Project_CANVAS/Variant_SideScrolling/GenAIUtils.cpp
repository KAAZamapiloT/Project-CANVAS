#include "GenAIUtils.h"

FString UGenAIUtils::CleanLLMResponse(FString RawResponse)
{
	if (RawResponse.IsEmpty()) return TEXT("{}");

	FString Clean = RawResponse;

	// --- Step A: Strip Markdown Code Blocks ---
	// Remove ```json and ```
	Clean.RemoveFromStart(TEXT("```json"));
	Clean.RemoveFromStart(TEXT("```"));
	Clean.RemoveFromEnd(TEXT("```"));
    
	// --- Step B: Remove "---" Delimiters (The Log Error Fix) ---
	// If the string contains dashed lines (often used by LLMs as separators), remove those lines.
	if (Clean.Contains(TEXT("---")))
	{
		TArray<FString> Lines;
		Clean.ParseIntoArrayLines(Lines);
		FString RebuiltString;
        
		for (const FString& Line : Lines)
		{
			// Only keep lines that are NOT purely dashed separators
			if (!Line.TrimStartAndEnd().StartsWith(TEXT("---")))
			{
				RebuiltString += Line + TEXT("\n"); // Add newline back
			}
		}
		Clean = RebuiltString;
	}

	// --- Step C: Extract the JSON Object ---
	// Find the FIRST '{' and the LAST '}' to ignore preamble text
	int32 StartIdx = -1;
	int32 EndIdx = -1;

	if (Clean.FindChar(TEXT('{'), StartIdx) && Clean.FindLastChar(TEXT('}'), EndIdx))
	{
		// Ensure End is after Start
		if (EndIdx > StartIdx)
		{
			Clean = Clean.Mid(StartIdx, (EndIdx - StartIdx) + 1);
		}
	}

	return Clean.TrimStartAndEnd();
}

TSharedPtr<FJsonObject> UGenAIUtils::StringToJsonObject(FString JsonString)
{
	TSharedPtr<FJsonObject> JsonObj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
	{
		return JsonObj;
	}
    
	// Log failure if needed, or return nullptr
	UE_LOG(LogTemp, Warning, TEXT("GenAIUtils: Failed to parse JSON string."));
	return nullptr;
}