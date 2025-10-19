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
		"{\"model\": \"llama3\", \"prompt\": \"%s\", \"stream\": false}"
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

	// 2. We need to parse the OLLAMA's JSON to get the LLM's JSON response
	TSharedPtr<FJsonObject> OllamaJsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
    
	if (FJsonSerializer::Deserialize(Reader, OllamaJsonObject) && OllamaJsonObject.IsValid())
	{
		// The LLM's response is inside the "response" field
		FString LlmResponseString = OllamaJsonObject->GetStringField(TEXT("response"));

		// 3. THIS IS THE KEY: We give the LLM's string to OUR parser
		FEnhancedScenePlan Plan = Parser->CreatePlan(LlmResponseString);

		// 4. FIRE THE EVENT! The "Painter" (ThemeManager) will hear this.
		OnThemeDataReady.Broadcast(Plan);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to parse Ollama's response: %s"), *ResponseString);
	}

	
}

FString UGenAISystem::ConstructMasterPrompt(FString UserPrompt)
{
	return FString::Printf(TEXT(
		"You are a game environment designer. "
		"The user wants a new theme for a wall. "
		"User request: '%s'. "
		"Your response MUST be in this exact JSON format: "
		"{"
		"  \"ThemeName\": \"A creative theme name\","
		"  \"BackgroundColor\": [R, G, B],"
		"  \"TextColor\": [R, G, B],"
		"  \"TextureName\": \"NameOfTexture\""
		"}"
		"Provide ONLY the JSON text and nothing else."
	), *UserPrompt);
}
