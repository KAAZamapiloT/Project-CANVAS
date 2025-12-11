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
		"\"model\":\"openai/gpt-oss-120b\","
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
	if (!bWasSuccessful&&!Response.IsValid())
	{
		UE_LOG(LogTemp,Error,TEXT("ULightingResolverLLM::Request Error"));
		return;
	}

	FString ResponseString = Response->GetContentAsString();
	UE_LOG(LogTemp, Warning, TEXT("ULightingResolverLLM:: RAW GROQ RESPONSE:\n%s"), *ResponseString);

    TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (FJsonSerializer::Deserialize(Reader, Object)&&Object.IsValid())
	{
		FString LLMResponseString ;
		const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
		if (Object->TryGetArrayField(TEXT("choices"), ChoicesArray) && ChoicesArray->Num() > 0)
		{
			TSharedPtr<FJsonObject> FirstChoice=(*ChoicesArray)[0]->AsObject();
			TSharedPtr<FJsonObject> MeesegeObj=FirstChoice->GetObjectField(TEXT("message"));
			LLMResponseString = MeesegeObj->GetStringField(TEXT("content"));
		}else
		{
			UE_LOG(LogTemp, Error, TEXT("ULightingResolverLLM:: No choices in Groq response!"));
			return;
		}
		int32 JsonStart=-1;
		int32 JsonEnd=-1;

		if (LLMResponseString.FindChar(TEXT('{'),JsonStart)&&
			(LLMResponseString.FindChar(TEXT('}'),JsonEnd))&&
			JsonStart < JsonEnd)
		{
			LLMResponseString=LLMResponseString.Mid(JsonStart,JsonEnd-JsonStart+1);
		}else
		{
			UE_LOG(LogTemp, Error, TEXT("ULIGHTINGRESOVERLLM::JSON parsing error: Could not find valid braces."));
			LLMResponseString = "";
		}

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
                
				// Trim and keep non-empty lines
				ProcessedLine = ProcessedLine.TrimStartAndEnd();
				if (!ProcessedLine.IsEmpty())
				{
					CleanedJSON += ProcessedLine + TEXT("\n");
				}
			}
			LLMResponseString = CleanedJSON.TrimStartAndEnd();
			
		}
		OnLightingPlanReady.Broadcast(LLMResponseString);
	}else
	{
		UE_LOG(LogTemp,Error,TEXT("ULightingResolverLLM::OnPlanReceived Error"));
	}

	
	
}

FString ULightingResolverLLM::ConstructPlanPrompt(FString UserPrompt,UAssetIndexer*Indexer)
{
	TArray<FString> Lines=Indexer->GetDiscoveredPostProcessNames();
	FString PPMString=FString::Join(Lines,TEXT("\" \""));
	FString Result =FString::Printf(TEXT(
	"You are an expert game environment designer specializing in Unreal Engine scenes.\n"
   "Generate a JSON scene plan based on the user request.\n\n"
   "USER REQUEST: \"%s\"\n\n"
"=== AVAILABLE SPAWNABLE ASSETS (Use for SpawnRequest.AssetPath) ===\n"
        "You may use EXACT names from EITHER list below:\n"
        "\n"
        "=== AVAILABLE POST-PROCESS MATERIALS (for Environment.PostProcessingName) ===\n"
        "[\"%s\"]\n\n"
        "== CRITICAL RULES ===\n"
        "1. RETURN ONLY VALID JSON - no markdown, code blocks, or explanations\n"
        
        "=== JSON SCHEMA ===\n"
        "\"Environment\": {\n"
		"    \"FogDensity\": 0.0-5.0,\n"
		"    \"FogColor\": [R:0-255, G:0-255, B:0-255],\n"
		"    \"PostProcessingName\": \"material_name\",\n"
		"    \"Lighting\": {\n"
		"      \"SunColor\": [R:0-255, G:0-255, B:0-255],\n"
		"      \"SunIntensity\": 0.0-50.0,\n"
		"      \"SunPitch\": -90.0 to 90.0,\n"
		"      \"SunYaw\": -360.0 to 360.0,\n"
		"      \"SkyLightColor\": [R:0-255, G:0-255, B:0-255],\n"
		"      \"SkyLightIntensity\": 0.0-10.0,\n"
		"      \"SunTemperature\": 1000-15000\n"
		"    }\n"
		"  },\n")
		,*UserPrompt,
        *PPMString
		);
  ;
    


	return Result;
}
