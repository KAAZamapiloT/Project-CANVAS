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
#include"RandomAssetSelector.h"
#include"API_KEY.h"

FString UTextureResolverLLM::CreateMasterPrompt(FString UserPrompt,UAssetIndexer* AssetIndexer)
{
	// [OPTIMIZATION] Use Base Names (Keys) instead of Full Paths
    // The LLM needs "Wood", not "/Game/Textures/T_Wood_BC.uasset"
    TArray<FString> AvailableMaterials = AssetIndexer->GetMaterialBaseNames(); 
    TArray<FString> AvailableTags = AssetIndexer->GetDiscoveredActorTags();
    
    // Join with quotes
    FString MaterialsString = FString::Join(AvailableMaterials, TEXT("\", \""));
    FString TagString = FString::Join(AvailableTags, TEXT("\", \""));

    FString MasterPrompt = FString::Printf(TEXT(
        "You are an expert game environment designer specializing in Unreal Engine scenes.\n"
        "Generate a JSON scene plan based on the user request.\n\n"
        "USER REQUEST: \"%s\"\n\n"
        
        "=== AVAILABLE ACTOR TAGS (for Props.TagName) ===\n"
        "Modify only actors with these tags:\n"
        "[\"%s\"]\n\n"
        
        "=== AVAILABLE TEXTURE BASE NAMES (for Props.Texture.BaseColorPath) ===\n"
        "Use these exact names. The system will auto-load the full PBR set (Normal, Roughness, etc.):\n"
        "[\"%s\"]\n\n"
      
        "== CRITICAL RULES ===\n"
        "1. MATCH THEME: If user asks for 'Cyberpunk', use neon/metal textures.\n"
        "2. TEXTURES: Use ONLY the base names listed above.\n"
        "3. TAGS: Use ONLY from AVAILABLE ACTOR TAGS.\n"
        "4. RETURN ONLY VALID JSON - no markdown.\n"
        "5. ALWAYS set bModifyProps to true if you change a texture.\n"
        "\n"
        "=== JSON SCHEMA ===\n"
        "{\n"
        "  \"ThemeName\": \"descriptive_name\",\n"
        "  \"bModifyProps\": true,\n"
        "  \"Props\": [\n"
        "    {\n"
        "      \"TagName\": \"exact_tag\",\n"
        "      \"PropColor\": [255, 255, 255],\n"
        "      \"Texture\": {\n"
        "        \"BaseColorPath\": \"material_base_name\"\n"
        "      }\n"
        "    }\n"
        "  ]\n"
        "}\n\n"
        "Generate the JSON now:"
    ),
    *UserPrompt,
    *TagString,
    *MaterialsString
    );

    return MasterPrompt;
}

void UTextureResolverLLM::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSucessfull)
{
	if (!bWasSucessfull || !Response.IsValid()) { OnTexturePlanReady.Broadcast("{}", ""); return; }

	FString ResponseString = Response->GetContentAsString();
	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid())
	{
		FString LLMResponseString;

		// 6. PARSE GEMINI RESPONSE STRUCTURE (candidates -> content -> parts -> text)
		const TArray<TSharedPtr<FJsonValue>>* Candidates;
		if (Object->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
		{
			TSharedPtr<FJsonObject> ContentObj = (*Candidates)[0]->AsObject()->GetObjectField(TEXT("content"));
			const TArray<TSharedPtr<FJsonValue>>* Parts;
			if (ContentObj->TryGetArrayField(TEXT("parts"), Parts) && Parts->Num() > 0)
			{
				LLMResponseString = (*Parts)[0]->AsObject()->GetStringField(TEXT("text"));
			}
		}
		else
		{
			// Log Raw Error if Gemini fails
			UE_LOG(LogTemp, Error, TEXT("TextureResolver (Gemini) Error: %s"), *ResponseString);
			OnTexturePlanReady.Broadcast("{}", "");
			return;
		}

		// 7. Prune & Broadcast
		USceneStateTracker* Tracker = UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<USceneStateTracker>();
		if (Tracker && Tracker->AssetIndexer)
		{
			FString Pruned = URandomAssetSelector::PruneTextureAssets(LLMResponseString, Tracker->AssetIndexer);
			UE_LOG(LogTemp, Log, TEXT("TextureResolver: Success! Pruned Context size: %d"), Pruned.Len());
			OnTexturePlanReady.Broadcast(LLMResponseString, Pruned);
		}
		else
		{
			OnTexturePlanReady.Broadcast(LLMResponseString, "");
		}
	}
	else
	{
		OnTexturePlanReady.Broadcast("{}", "");
	}

	
}

void UTextureResolverLLM::RequestPlan(FString UserPrompt, UWorld* World, class USceneHistoryManager* HistoryManager)
{
	if (!World || !IsValid(World))
	{
		UE_LOG(LogTemp, Error, TEXT("TextureResolver: Invalid World Context"));
		OnTexturePlanReady.Broadcast("{}", "");
		return;
	}
	USceneStateTracker* Tracker = UGameplayStatics::GetGameInstance(World)->GetSubsystem<USceneStateTracker>();
	if (!Tracker || !Tracker->AssetIndexer) return;
	
	FString PlanPrompt=CreateMasterPrompt(UserPrompt,Tracker->AssetIndexer);
	// 3. SWITCH TO GEMINI KEY
	FString GeminiKey = API_KEY::GemKey(); 

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    
	// 4. USE GEMINI URL
	FString Url = FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=%s"), *GeminiKey);
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// 5. USE GEMINI PAYLOAD STRUCTURE
	FString Payload = FString::Printf(TEXT(
	   "{"
	   "  \"contents\": [{"
	   "    \"parts\": [{"
	   "      \"text\": \"%s\""
	   "    }]"
	   "  }],"
	   "  \"generationConfig\": {"
	   "    \"temperature\": 0.25,"
	   "    \"responseMimeType\": \"application/json\"" 
	   "  }"
	   "}"
	), *PlanPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

	Request->SetContentAsString(Payload);
	Request->OnProcessRequestComplete().BindUObject(this, &UTextureResolverLLM::OnResponseReceived);
	Request->ProcessRequest();
}
