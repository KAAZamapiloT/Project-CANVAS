// Fill out your copyright notice in the Description page of Project Settings.


#include "MeshResolverLLM.h"

#include "AssetIndexer.h"
#include "LocalizationDescriptor.h"
#include "SceneStateTracker.h"
#include "RandomAssetSelector.h"
#include "Kismet/GameplayStatics.h"

FString UMeshResolverLLM::CreateMeshPayload(FString UserPrompt, UAssetIndexer* Indexer)
{
    TArray<FString> Meshes = Indexer->GetSmartMeshList(); 
    TArray<FString> Particles = Indexer->GetDiscoveredParticleNames();

    FString MeshString = FString::Join(Meshes, TEXT("\", \""));
    FString ParticleString = FString::Join(Particles, TEXT("\", \""));

    FString Payload = FString::Printf(TEXT(
        "ROLE: You are the Lead Structural Engineer & Layout Specialist for a game scene.\n"
        "TASK: Analyze the Client Request and generate a layout plan using ONLY the provided asset catalog.\n\n"
        
        "CLIENT REQUEST: \"%s\"\n\n"
        
        "=== CATALOG OF APPROVED ASSETS (Inventory) ===\n"
        "You must select assets exclusively from these lists. Do not hallucinate files.\n\n"
        
        "--- 1. STATIC MESH GROUPS (Solid Objects) ---\n"
        "Use these for 'SpawnRequest'.\n"
        "[\"%s\"]\n\n"
        
        "--- 2. PARTICLE SYSTEMS (Atmosphere/FX) ---\n"
        "Use these for 'ParticleSpawns'.\n"
        "[\"%s\"]\n\n"
        
        "=== SPATIAL STRATEGY (Player-Relative Coordinates) ===\n"
        "The Player is at (0,0,0). Use these distinct zones:\n"
        "- CENTER: (0 to 300) units. The focal point/Hero prop.\n"
        "- BACKGROUND: (+500 to +1500) units Y-axis (Away from player). Large structures.\n"
        "- FOREGROUND: (-200 to 0) units Y-axis (Between camera and player). Small debris.\n"
        "- LEFT_SIDE: (-500 to -200) units X-axis.\n"
        "- RIGHT_SIDE: (+200 to +500) units X-axis.\n\n"

        "=== DATA INTEGRITY RULES ===\n"
        "1. SEGREGATION: Solid objects -> 'SpawnRequest'. FX -> 'ParticleSpawns'.\n"
        "2. ROTATION: Use [Pitch, Yaw, Roll] in degrees. (Yaw = Z-axis rotation).\n"
        "3. SCALE: Realistic sizing.\n"
        "   - Buildings: 1.5 to 3.0\n"
        "   - Props: 0.8 to 1.2\n"
        "4. TAGS: Use format 'GenAI.Structure.[ObjectName]' or 'GenAI.FX.[ObjectName]'.\n"
        "5. OUTPUT FORMAT: Return strictly valid JSON matching the schema below.\n"
        "\n"
        "=== JSON SCHEMA ===\n"
        "{\n"
        "  \"ThemeName\": \"short_descriptive_name\",\n"
        "  \"SpawnRequest\": [\n"
        "    {\n"
        "      \"AssetPath\": \"EXACT_NAME_FROM_CATALOG\",\n"
        "      \"ObjectName\": \"unique_id_string\",\n"
        "      \"LocationName\": \"SEMANTIC_ZONE_NAME\",\n"
        "      \"LocationOffset\": [X, Y, Z],\n"
        "      \"Rotation\": [Pitch, Yaw, Roll],\n"
        "      \"Scale\": [X, Y, Z],\n"
        "      \"Tag\": \"GenAI.Type.Name\",\n"
        "      \"ClearanceRadius\": 150.0\n"
        "    }\n"
        "  ],\n"
        "  \"ParticleSpawns\": [\n"
        "    {\n"
        "      \"AssetPath\": \"EXACT_NAME_FROM_PARTICLE_LIST\",\n"
        "      \"ObjectName\": \"unique_fx_id\",\n"
        "      \"LocationName\": \"SEMANTIC_ZONE_NAME\",\n"
        "      \"LocationOffset\": [0, 0, 0],\n"
        "      \"Rotation\": [0, 0, 0],\n"
        "      \"Scale\": [1, 1, 1],\n"
        "      \"Tag\": \"GenAI.FX.Name\",\n"
        "      \"ClearanceRadius\": 150.0\n"
        "    }\n"
        "  ]\n"
        "}\n\n"
        "GENERATE STRUCTURAL PLAN:"
    ),
    *UserPrompt,
    *MeshString,
    *ParticleString
    );

    return Payload;
}

FString UMeshResolverLLM::CreatePrunedMeshPayload(FString UserPrompt, FString& PrunedAssets)
{
	// 1. Parse String back to Array
	TArray<FString> RawAssets;
	PrunedAssets.ParseIntoArray(RawAssets, TEXT(","), true);

	// 2. Split Assets into Categories
	TArray<FString> MeshList;
	TArray<FString> ParticleList;

	for (FString& Path : RawAssets)
	{
		Path.TrimStartAndEndInline(); // Clean up spaces " Asset" -> "Asset"
        
		// Simple heuristic: Check path or naming convention
		if (Path.Contains(TEXT("/Particles/")) || Path.Contains(TEXT("Niagara")) || Path.Contains(TEXT("NS_")) || Path.Contains(TEXT("P_")))
		{
			ParticleList.Add(Path);
		}
		else
		{
			MeshList.Add(Path);
		}
	}

	FString MeshString = FString::Join(MeshList, TEXT("\", \""));
	FString ParticleString = FString::Join(ParticleList, TEXT("\", \""));
	FString Payload = FString::Printf(TEXT(
        "ROLE: You are the Lead Structural Engineer & Layout Specialist for a game scene.\n"
        "TASK: Analyze the Client Request and generate a layout plan using ONLY the provided asset catalog.\n\n"
        
        "CLIENT REQUEST: \"%s\"\n\n"
        
        "=== CATALOG OF APPROVED ASSETS (Inventory) ===\n"
        "You must select assets exclusively from these lists. Do not hallucinate files.\n\n"
        
        "--- 1. STATIC MESH GROUPS (Solid Objects) ---\n"
        "Use these for 'SpawnRequest'.\n"
        "[\"%s\"]\n\n"
        
        "--- 2. PARTICLE SYSTEMS (Atmosphere/FX) ---\n"
        "Use these for 'ParticleSpawns'.\n"
        "[\"%s\"]\n\n"
        
        "=== SPATIAL STRATEGY (Player-Relative Coordinates) ===\n"
        "The Player is at (0,0,0). Use these distinct zones:\n"
        "- CENTER: (0 to 300) units. The focal point/Hero prop.\n"
        "- BACKGROUND: (+500 to +1500) units Y-axis (Away from player). Large structures.\n"
        "- FOREGROUND: (-200 to 0) units Y-axis (Between camera and player). Small debris.\n"
        "- LEFT_SIDE: (-500 to -200) units X-axis.\n"
        "- RIGHT_SIDE: (+200 to +500) units X-axis.\n\n"

        "=== DATA INTEGRITY RULES ===\n"
        "1. SEGREGATION: Solid objects -> 'SpawnRequest'. FX -> 'ParticleSpawns'.\n"
        "2. ROTATION: Use [Pitch, Yaw, Roll] in degrees. (Yaw = Z-axis rotation).\n"
        "3. SCALE: Realistic sizing.\n"
        "   - Buildings: 1.5 to 3.0\n"
        "   - Props: 0.8 to 1.2\n"
        "4. TAGS: Use format 'GenAI.Structure.[ObjectName]' or 'GenAI.FX.[ObjectName]'.\n"
        "5. OUTPUT FORMAT: Return strictly valid JSON matching the schema below.\n"
        "\n"
        "=== JSON SCHEMA ===\n"
        "{\n"
        "  \"ThemeName\": \"short_descriptive_name\",\n"
        "  \"SpawnRequest\": [\n"
        "    {\n"
        "      \"AssetPath\": \"EXACT_NAME_FROM_CATALOG\",\n"
        "      \"ObjectName\": \"unique_id_string\",\n"
        "      \"LocationName\": \"SEMANTIC_ZONE_NAME\",\n"
        "      \"LocationOffset\": [X, Y, Z],\n"
        "      \"Rotation\": [Pitch, Yaw, Roll],\n"
        "      \"Scale\": [X, Y, Z],\n"
        "      \"Tag\": \"GenAI.Type.Name\",\n"
        "      \"ClearanceRadius\": 150.0\n"
        "    }\n"
        "  ],\n"
        "  \"ParticleSpawns\": [\n"
        "    {\n"
        "      \"AssetPath\": \"EXACT_NAME_FROM_PARTICLE_LIST\",\n"
        "      \"ObjectName\": \"unique_fx_id\",\n"
        "      \"LocationName\": \"SEMANTIC_ZONE_NAME\",\n"
        "      \"LocationOffset\": [0, 0, 0],\n"
        "      \"Rotation\": [0, 0, 0],\n"
        "      \"Scale\": [1, 1, 1],\n"
        "      \"Tag\": \"GenAI.FX.Name\",\n"
        "      \"ClearanceRadius\": 150.0\n"
        "    }\n"
        "  ]\n"
        "}\n\n"
        "GENERATE STRUCTURAL PLAN:"
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

void UMeshResolverLLM::RequestPlan_Pruned(FString UserPrompt, FString& PrunedAssets, UWorld* World, USceneHistoryManager* HistoryManager)
{
	// Generate the specific payload
	FString PlanPrompt = CreatePrunedMeshPayload(UserPrompt, PrunedAssets);

	FString KEY = API_KEY::GetKey(); // Use Groq/Ollama Key

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.groq.com/openai/v1/chat/completions")); 
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *KEY));

	FString Payload = FString::Printf(TEXT(
		"{"
		"\"model\":\"llama-3.3-70b-versatile\","
		"\"messages\":["
			"{\"role\":\"system\",\"content\":\"You are a JSON generator. Respond only with valid JSON.\"},"
			"{\"role\":\"user\",\"content\":\"%s\"}"
		"],"
		"\"temperature\":0.2,"
		"\"max_tokens\":4000"
		"}"
	), *PlanPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

	Request->SetContentAsString(Payload);
	Request->OnProcessRequestComplete().BindUObject(this, &UMeshResolverLLM::OnPlanReceived);
	Request->ProcessRequest();
}