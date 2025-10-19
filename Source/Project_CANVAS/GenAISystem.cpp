// Fill out your copyright notice in the Description page of Project Settings.


#include "GenAISystem.h"

#include "JsonParser.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"

void UGenAISystem::RequestSceneChange(FString UserPrompt)
{
	// having  a parser 
	if (!Parser)
	{
		Parser=NewObject<UJsonParser>();
	}
	// constructing master prompt for model
	FString MasterPrompt = ConstructMasterPrompt(UserPrompt);

// creating a HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://localhost:11434/api/generate")); // Default Ollama URL
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// 4. Create the JSON payload for Ollama
	FString Payload = FString::Printf(TEXT(
		"{\"model\": \"gemma3:1b-it-qat\", \"prompt\": \"%s\", \"stream\": false}"
	), *MasterPrompt.Replace(TEXT("\""), TEXT("\\\""))); // Escape quotes in the prompt

	
	Request->SetContentAsString(Payload);

	// 5. Bind the callback function (what to do when we get a response)
	Request->OnProcessRequestComplete().BindUObject(this, &UGenAISystem::OnOllamaResponseReceived);
    
	// 6. Send the request!
	Request->ProcessRequest();
	
}

void UGenAISystem::OnOllamaResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Ollama request failed!"));
		return;
	}
	FString ResponseString = Response->GetContentAsString();
        UE_LOG(LogTemp, Warning, TEXT("RAW OLLAMA RESPONSE:\n%s"), *ResponseString);
    
        TSharedPtr<FJsonObject> OllamaJsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
    
        if (FJsonSerializer::Deserialize(Reader, OllamaJsonObject) && OllamaJsonObject.IsValid())
        {
            FString LlmResponseString = OllamaJsonObject->GetStringField(TEXT("response"));
            UE_LOG(LogTemp, Log, TEXT("Extracted LLM response (raw): %s"), *LlmResponseString);
    
            // --- NEW, MORE ROBUST CLEANING ---
            int32 JsonStart = -1;
            int32 JsonEnd = -1;
    
            // Find the first '{'
            if (LlmResponseString.FindChar(TEXT('{'), JsonStart))
            {
                // Find the last '}'
                if (LlmResponseString.FindLastChar(TEXT('}'), JsonEnd))
                {
                    // Extract the substring between the first { and the last } (inclusive)
                    if (JsonStart < JsonEnd)
                    {
                        LlmResponseString = LlmResponseString.Mid(JsonStart, (JsonEnd - JsonStart) + 1);
                        LlmResponseString = LlmResponseString.TrimStartAndEnd(); // Trim any extra whitespace
                        UE_LOG(LogTemp, Log, TEXT("Cleaned LLM response for parser: %s"), *LlmResponseString);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("JSON parsing error: Found braces in wrong order."));
                        LlmResponseString = ""; // Clear string on error
                    }
                }
                else
                {
                     UE_LOG(LogTemp, Error, TEXT("JSON parsing error: Could not find closing brace '}'."));
                     LlmResponseString = ""; // Clear string on error
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("JSON parsing error: Could not find opening brace '{'."));
                LlmResponseString = ""; // Clear string on error
            }
            // ------------------------------------
    
            // Pass the CLEANED string (or empty if cleaning failed) to the parser
            FEnhancedScenePlan Plan = Parser->CreatePlan(LlmResponseString); // CreatePlan handles empty string
    
            // Log the parsed plan data
            UE_LOG(LogTemp, Warning, TEXT("GENAI: Parsed Plan - Theme: %s, Color: %s, Texture: %s"),
                    *Plan.ThemeName, *Plan.BackgroundColor.ToString(), *Plan.TextureName);
    
            OnThemeDataReady.Broadcast(Plan);
            UE_LOG(LogTemp, Warning, TEXT("GENAI: Broadcast OnThemeDataReady"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to parse Ollama's MAIN response: %s"), *ResponseString);
        }

	
}

FString UGenAISystem::ConstructMasterPrompt(FString UserPrompt)
{
	FString AvailableTextures = TEXT(
		"[" "\"T_GridChecker_A\",]"
	);
	return FString::Printf(TEXT(
		"You are a game designer painting a wall. "
		"User request: '%s'. "
		"You MUST choose ONE texture name from this exact list: %s" // Be explicit: ONE name
		"Your response MUST be in this exact JSON format. "
		"The BackgroundColor and TextColor values MUST be arrays of three NUMBERS between 0 and 255 (RGB). Example: [255, 0, 128]. " // Specify NUMBERS
		"The TextureName MUST be ONE string chosen EXACTLY from the provided list. " // Reinforce texture rule

		"{"
		"  \"ThemeName\": \"A creative theme name\","
		"  \"BackgroundColor\": [R, G, B]," // Use numbers 0-255
		"  \"TextColor\": [R, G, B],"       // Use numbers 0-255
		"  \"TextureName\": \"(a single texture name chosen ONLY from the list)\""
		"}"
		"Provide ONLY the JSON text inside the curly braces {} and nothing else." // Tell it NO markdown

	), *UserPrompt,*AvailableTextures);
}
