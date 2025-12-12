// Fill out your copyright notice in the Description page of Project Settings.


#include "MeshResolverLLM.h"

#include "AssetIndexer.h"
#include "LocalizationDescriptor.h"
#include "SceneStateTracker.h"
#include "RandomAssetSelector.h"
#include "Kismet/GameplayStatics.h"

FString UMeshResolverLLM::CreateMeshPayload(FString UserPrompt, UAssetIndexer*Indexer)
{

	// Use the smart list which prioritizes Group Names ("Chair") over specific variants ("Chair_01")
    // This reduces token usage and improves LLM reasoning.
    TArray<FString> Meshes = Indexer->GetSmartMeshList(); 
    TArray<FString> Particles = Indexer->GetDiscoveredParticleNames();

    FString MeshString = FString::Join(Meshes, TEXT("\", \""));
    FString ParticleString = FString::Join(Particles, TEXT("\", \""));

    FString Payload = FString::Printf(TEXT(
        "You are an expert game environment designer specializing in Unreal Engine scenes.\n"
        "Generate a JSON scene plan based on the user request.\n\n"
        "USER REQUEST: \"%s\"\n\n"
       "=== AVAILABLE SPAWNABLE ASSETS (Use for SpawnRequest.AssetPath) ===\n"
        "You may use EXACT names from EITHER list below:\n"
        "\n"
        "--- STATIC MESHES (Object Groups) ---\n"
        "[\"%s\"]\n"
        "\n"
        "--- PARTICLE EFFECTS ---\n"
        "[\"%s\"]\n"
        "\n"
        "== CRITICAL RULES ===\n"
        "1. VARIETY: If you select a group name like 'Chair', the system will automatically pick random variants (Chair_01, Chair_02).\n"
        "2. SPLIT ASSETS: Meshes -> 'SpawnRequest', Particles -> 'ParticleSpawn'.\n"
        "3. LOCATIONS: Distribute objects across CENTER, BACKGROUND, LEFT_SIDE, RIGHT_SIDE.\n"
        "4. SCALE: Use reasonable scales [0.8 - 2.5]. No leading zeros.\n"
        "\n"
        "=== JSON SCHEMA ===\n"
        "{\n"
        "  \"ThemeName\": \"descriptive_name\",\n"
        "  \"SpawnRequest\": [\n"
        "    {\n"
        "      \"AssetPath\": \"exact_mesh_name\",\n"
        "      \"ObjectName\": \"unique_instance_name\",\n"
        "      \"LocationName\": \"SEMANTIC_LOCATION\",\n"
        "      \"LocationOffset\": [0, 0, 0],\n"
        "      \"Rotation\": [0, 0, 0],\n"
        "      \"Scale\": [1, 1, 1],\n"
        "      \"ClearanceRadius\": 150\n"
        "    }\n"
        "  ],\n"
        "  \"ParticleSpawn\": []\n"
        "}\n\n"
        "Generate the JSON now:"
    ),
    *UserPrompt,
    *MeshString,
    *ParticleString
    );

    return Payload;
}

void UMeshResolverLLM::RequestPlan(FString UserPrompt, UWorld* World, class USceneHistoryManager* HistoryManager)
{
	USceneStateTracker*Tracker=UGameplayStatics::GetGameInstance(World)->GetSubsystem<USceneStateTracker>();
	FString PlanPrompt=CreateMeshPayload(UserPrompt,Tracker->AssetIndexer);

	FString KEY=API_KEY::GetKey();

	TSharedRef<IHttpRequest,ESPMode::ThreadSafe>Request=FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.groq.com/openai/v1/chat/completions")); // Changed URL
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *KEY)); // Added auth

	FString Payload = FString::Printf(TEXT(
		"{"
		"\"model\":\"llama-3.3-70b-versatile\","
		"\"messages\":["
			"{\"role\":\"system\",\"content\":\"You are a JSON generator for a 3D scene builder. Only respond with valid JSON, no markdown, no code blocks.\"},"
			"{\"role\":\"user\",\"content\":\"%s\"}"
		"],"
		"\"temperature\":0.25,"
		"\"max_tokens\":8000"
		"}"
	), *PlanPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

	Request->SetContentAsString(Payload);

	Request->OnProcessRequestComplete().BindUObject(this, &UMeshResolverLLM::OnPlanReceived);
	
	Request->ProcessRequest();
	
}

void UMeshResolverLLM::OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("UMeshResolverLLM: HTTP Request Failed"));
		OnMeshPlanReady.Broadcast("{}", "");
		return;
	}
	FString ResponseString = Response->GetContentAsString();

	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

	if (FJsonSerializer::Deserialize(Reader, Object)&&Object.IsValid())
	{
		FString LLMResponseString;

		const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;

		if (Object->TryGetArrayField(TEXT("choices"), ChoicesArray)&&ChoicesArray->Num()>0)
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
		
       
		// === INTEGRATION ===
		USceneStateTracker* Tracker = UGameplayStatics::GetGameInstance(GetWorld())->GetSubsystem<USceneStateTracker>();
		if (Tracker && Tracker->AssetIndexer)
		{
			// 1. PRUNE: This calls PruneMeshAssets -> AssetIndexer->GetTopKMeshesForQuery
			// This expands "Chair" into "Chair_01, Chair_02, Chair_03" for the Master Planner
			FString Choices = URandomAssetSelector::PruneMeshAssets(LLMResponseString, Tracker->AssetIndexer);
        
			UE_LOG(LogTemp, Log, TEXT("MeshResolver: Pruned Context -> [%s]"), *Choices);

			// 2. BROADCAST
			OnMeshPlanReady.Broadcast(LLMResponseString, Choices);
		}
	}

	
}
