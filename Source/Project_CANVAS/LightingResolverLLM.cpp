// Fill out your copyright notice in the Description page of Project Settings.


#include "LightingResolverLLM.h"
#include "SceneStateTracker.h"
//#include "JsonParser.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "API_KEY.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "AssetIndexer.h"
void ULightingResolverLLM::RequestPlan(FString UserPrompt, UWorld* World, class USceneHistoryManager* HistoryManager)
{

    USceneStateTracker*Tracker=UGameplayStatics::GetGameInstance(World)->GetSubsystem<USceneStateTracker>();

	FString PlanPrompt=ConstructPlanPrompt(UserPrompt,Tracker->AssetIndexer);

	FString Key=API_KEY::GetKey();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.groq.com/openai/v1/chat/completions")); // Changed URL
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *Key)); // Added auth

	
FString Payload = FString::Printf(TEXT(
	
		"{"
		"\"model\":\"llama-3.3-70b-versatile\","
		"\"messages\":["
			"{\"role\":\"system\",\"content\":\"You are a JSON generator for a 3D scene builder. Only respond with valid JSON, no markdown, no code blocks.\"},"
			"{\"role\":\"user\",\"content\":\"%s\"}"
		"],"
		"\"temperature\":0.3,"
		"\"max_tokens\":1000"
		"}"
	), *PlanPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

	Request->SetContentAsString(Payload);

	Request->OnProcessRequestComplete().BindUObject(this,&ULightingResolverLLM::OnPlanReceived);
	
	Request->ProcessRequest();

	
}

void ULightingResolverLLM::OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    // 1. Validate Request
    if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
    {
        UE_LOG(LogTemp, Error, TEXT("ULightingResolverLLM::Request Error: %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
        OnLightingPlanReady.Broadcast("{}"); // Unblock barrier
        return;
    }

    FString ResponseString = Response->GetContentAsString();
    
    // 2. Parse API Response
    TSharedPtr<FJsonObject> Object;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

    if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid())
    {
        FString LLMResponseString;
        const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
        
        // Handle Groq/OpenAI Structure
        if (Object->TryGetArrayField(TEXT("choices"), ChoicesArray) && ChoicesArray->Num() > 0)
        {
            TSharedPtr<FJsonObject> FirstChoice = (*ChoicesArray)[0]->AsObject();
            TSharedPtr<FJsonObject> MessageObj = FirstChoice->GetObjectField(TEXT("message"));
            LLMResponseString = MessageObj->GetStringField(TEXT("content"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ULightingResolverLLM: 'choices' field missing."));
            OnLightingPlanReady.Broadcast("{}");
            return;
        }

        // 3. Extract JSON Block
        int32 JsonStart = -1;
        int32 JsonEnd = -1;

        if (LLMResponseString.FindChar(TEXT('{'), JsonStart) && 
            LLMResponseString.FindLastChar(TEXT('}'), JsonEnd) && 
            JsonStart < JsonEnd)
        {
            LLMResponseString = LLMResponseString.Mid(JsonStart, JsonEnd - JsonStart + 1);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("ULightingResolverLLM: Could not find valid JSON braces."));
            // Fallback: If no JSON found, send empty object to keep pipeline moving
            OnLightingPlanReady.Broadcast("{}"); 
            return;
        }

        // 4. Clean Comments
        if (!LLMResponseString.IsEmpty())
        {
            TArray<FString> Lines;
            LLMResponseString.ParseIntoArrayLines(Lines);
            FString CleanedJSON;

            for (const FString& Line : Lines)
            {
                FString ProcessedLine = Line;
                int32 CommentIndex = ProcessedLine.Find(TEXT("//"));
                if (CommentIndex != INDEX_NONE)
                {
                    ProcessedLine = ProcessedLine.Left(CommentIndex);
                }
                
                ProcessedLine = ProcessedLine.TrimStartAndEnd();
                if (!ProcessedLine.IsEmpty())
                {
                    CleanedJSON += ProcessedLine + TEXT("\n");
                }
            }
            LLMResponseString = CleanedJSON.TrimStartAndEnd();
        }

        // 5. Broadcast Success
        OnLightingPlanReady.Broadcast(LLMResponseString);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ULightingResolverLLM: Failed to deserialize API response"));
        OnLightingPlanReady.Broadcast("{}");
    }
}

FString ULightingResolverLLM::ConstructPlanPrompt(FString UserPrompt, UAssetIndexer* Indexer)
{
	// 1. Prepare Data
	TArray<FString> Lines = Indexer->GetDiscoveredPostProcessNames();
	// Fix join delimeter to include commas: "A", "B", "C"
	FString PPMString = FString::Join(Lines, TEXT("\", \"")); 

	// 2. Build Prompt
	// CRITICAL FIX: The Schema string must start with '{' and end with '}'
	FString Result = FString::Printf(TEXT(
		"You are an expert game lighting artist specializing in Unreal Engine 5.\n"
		"Generate a JSON configuration for the environment based on this request: \"%s\"\n\n"

		"=== AVAILABLE POST-PROCESS MATERIALS ===\n"
		"If the theme requires it, use one of these EXACT names. Otherwise use \"\":\n"
		"[\"%s\"]\n\n"

		"=== RULES ===\n"
		"1. Output ONLY valid JSON. No markdown code blocks.\n"
		"2. Ensure color values are [R, G, B] integers (0-255).\n"
		"3. Ensure numeric values are within valid ranges.\n"
		"4. If theme is 'Night' or 'Dark', set SunIntensity low (0.5-2.0) and FogDensity high.\n"
        
		"=== JSON SCHEMA ===\n"
		"{\n"
		"  \"Environment\": {\n"
		"    \"FogDensity\": 0.0-5.0,\n"
		"    \"FogColor\": [R, G, B],\n"
		"    \"PostProcessingName\": \"material_name\",\n"
		"    \"Lighting\": {\n"
		"      \"SunColor\": [R, G, B],\n"
		"      \"SunIntensity\": 0.0-20.0,\n"
		"      \"SunPitch\": -90.0 to 90.0,\n"
		"      \"SunYaw\": 0.0 to 360.0,\n"
		"      \"SkyLightColor\": [R, G, B],\n"
		"      \"SkyLightIntensity\": 0.0-10.0,\n"
		"      \"SunTemperature\": 1500-12000\n"
		"    }\n"
		"  }\n"
		"}"
	), 
	*UserPrompt,
	*PPMString
	);

	return Result;
}