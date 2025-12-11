// Fill out your copyright notice in the Description page of Project Settings.


#include "TextureResolverLLM.h"
//#include "JsonParser.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "AssetIndexer.h"
#include "SceneStateTracker.h"
#include"API_KEY.h"

FString UTextureResolverLLM::CreateMasterPrompt(FString UserPrompt,UAssetIndexer* AssetIndexer)
{
	TArray<FString> AvailibleTextures=AssetIndexer->GetDiscoveredTextureNames();
	TArray<FString> AvalibleTags=AssetIndexer->GetDiscoveredActorTags();
	FString TexturesString=FString::Join((AvailibleTextures),TEXT("\", \""));
	FString TagString=FString::Join((AvalibleTags),TEXT("\", \""));
	FString MasterPrompt=
		FString::Printf(TEXT(
        "You are an expert game environment designer specializing in Unreal Engine scenes.\n"
        "Generate a JSON scene plan based on the user request.\n\n"
        "USER REQUEST: \"%s\"\n\n"
        "=== AVAILABLE ACTOR TAGS (for Props.TagName) ===\n"
        "Modify only actors with these tags:\n"
        "[\"%s\"]\n\n"
        
        "=== AVAILABLE Textures(for Props.Texture.BaseColorPath) ===\n"
        "Use these material base names (system auto-loads PBR textures):\n"
        "[\"%s\"]\n\n"
      
        "== CRITICAL RULES ===\n"
		"1. If no suitable assets are available, set bSpawnActors to false.\n"
        "2. TEXTURES: Use ONLY base names (NOT full paths)\n"
        "   System handles loading: material_name_diff_2k, material_name_rough_2k, etc.\n"
        "3. TAGS: Use ONLY from AVAILABLE ACTOR TAGS for modification\n"
        "4. RETURN ONLY VALID JSON - no markdown, code blocks, or explanations\n"
        "\n"
        "5.ALWAYS set bModifyProps to true if the theme requires a material change (e.g. converting concrete walls to wood)."
        "\n"
        "=== JSON SCHEMA ===\n"
        "{\n"
        "  \"ThemeName\": \"descriptive_name\",\n"
        "  \"bModifyProps\": true/false,\n"
        "  \"Props\": [\n"
        "    {\n"
        "      \"TagName\": \"exact_tag\",\n"
        "      \"PropColor\": [R:0-255, G:0-255, B:0-255],\n"
        "      \"Texture\": {\n"
        "        \"BaseColorPath\": \"material_base_name\",\n"
        "        \"NormalPath\": \"\",\n"
        "        \"RoughnessPath\": \"\",\n"
        "        \"MetallicPath\": \"\"\n"
        "      },\n"
        "      \"ParticleEffects\": \"\"\n"
        "    }\n"
        "  ],\n"
        "}\n\n"
        "Generate the JSON now:"
    ),
    *UserPrompt ,
    *TagString,
    *TexturesString
    );

	return MasterPrompt;
}

void UTextureResolverLLM::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSucessfull)
{
	if (!bWasSucessfull&&!Response.IsValid())
	{
		UE_LOG(LogTemp,Error,TEXT("%s"),TEXT("UTextureResolverLLM:Error receiving response"));
		return;
	}
	FString ResponseString = Response->GetContentAsString();

	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (FJsonSerializer::Deserialize(Reader, Object)&&Object.IsValid())
	{
		FString LLMResponseString;

		const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;

		if (Object->TryGetArrayField(TEXT("Choices"), ChoicesArray)&&ChoicesArray->Num()>0)
		{
			TSharedPtr<FJsonObject> FirstChoice=(*ChoicesArray)[0]->AsObject();
			TSharedPtr<FJsonObject> MessegeObj=FirstChoice->GetObjectField(TEXT("message"));

			LLMResponseString=MessegeObj->GetStringField(TEXT("content"));
		}else
		{
			UE_LOG(LogTemp,Error,TEXT("TextureResolverLLM::OnResponseReceived error"));
return;
		}
		int32 JsonStart=-1;
		int32 JsonEnd=-1;

		if (LLMResponseString.FindChar(TEXT('{'),JsonStart)&&
			LLMResponseString.FindChar(TEXT('}'),JsonEnd)&&
			JsonStart<JsonEnd)
		{
			LLMResponseString=LLMResponseString.Mid(JsonStart,JsonEnd-JsonStart+1);
		}else
		{
			UE_LOG(LogTemp, Error, TEXT("JSON parsing error: Could not find valid braces."));
			LLMResponseString = "";
		}

if (!LLMResponseString.IsEmpty())
{
	TArray<FString> Lines;
	LLMResponseString.ParseIntoArrayLines(Lines);
	FString CleanedJSON;

	for (const FString& Line:Lines)
	{
		FString ProcessedLine=Line;
		int32 CommentIndex=ProcessedLine.Find(TEXT("//"));
		if (CommentIndex!=INDEX_NONE)
		{
			ProcessedLine=ProcessedLine.Left(CommentIndex);
		}
		ProcessedLine=ProcessedLine.TrimStartAndEnd();

		if (!ProcessedLine.IsEmpty())
		{
			CleanedJSON+=ProcessedLine+TEXT("\n");
		}
	}
	LLMResponseString=CleanedJSON.TrimStartAndEnd();
	
}
		// TODO :  GET PRUNED CONTEXT 
TArray<FString> Tags={"lets see"};
		OnTexturePlanReady.Broadcast(LLMResponseString,Tags);
	}

	

	
}

void UTextureResolverLLM::RequestPlan(FString UserPrompt, UWorld* World, class USceneHistoryManager* HistoryManager)
{
	USceneStateTracker* Tracker=UGameplayStatics::GetGameInstance(World)->GetSubsystem<USceneStateTracker>();
	if (!Tracker)
	{
		return;
	}
	
	FString PlanPrompt=CreateMasterPrompt(UserPrompt,Tracker->AssetIndexer);
	FString KEY=API_KEY::GetKey();

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.groq.com/openai/v1/chat/completions")); // Changed URL
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *KEY)); // Added auth
	
	FString Payload = FString::Printf(TEXT(
		"{"
		"\"model\":\"openai/gpt-oss-120b\","
		"\"messages\":["
			"{\"role\":\"system\",\"content\":\"You are a JSON generator for a 3D scene builder. Only respond with valid JSON, no markdown, no code blocks.\"},"
			"{\"role\":\"user\",\"content\":\"%s\"}"
		"],"
		"\"temperature\":0.2,"
		"\"max_tokens\":7000"
		"}"
	), *PlanPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

	Request->SetContentAsString(Payload);

    Request->OnProcessRequestComplete().BindUObject(this, &UTextureResolverLLM::OnResponseReceived);
	
	Request->ProcessRequest();
}
