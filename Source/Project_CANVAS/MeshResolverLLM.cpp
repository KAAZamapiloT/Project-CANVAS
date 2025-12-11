// Fill out your copyright notice in the Description page of Project Settings.


#include "MeshResolverLLM.h"

#include "AssetIndexer.h"
#include "LocalizationDescriptor.h"
#include "SceneStateTracker.h"
#include "Kismet/GameplayStatics.h"

FString UMeshResolverLLM::CreateMeshPayload(FString UserPrompt, UAssetIndexer*Indexer)
{

	FString MeshString=FString::Join(Indexer->GetAllMeshNames(),TEXT("\", \""));
	FString ParticleString=FString::Join(Indexer->GetDiscoveredParticleNames(),TEXT("\", \""));
	FString Payload = FString::Printf(TEXT(
        "You are an expert game environment designer specializing in Unreal Engine scenes.\n"
        "Generate a JSON scene plan based on the user request.\n\n"
        "USER REQUEST: \"%s\"\n\n"
       "=== AVAILABLE SPAWNABLE ASSETS (Use for SpawnRequest.AssetPath) ===\n"
        "You may use EXACT names from EITHER list below:\n"
        "\n"
        "--- STATIC MESHES ---\n"
        "%s\n"
        "\n"
        "--- PARTICLE EFFECTS ---\n"
        "%s\n"
        "\n"
        "== CRITICAL RULES ===\n"
		"1. SPLIT ASSETS:\n"
		"   - STATIC MESHES go into 'SpawnRequest' array.\n"
		"   - PARTICLE EFFECTS go into 'ParticleSpawns' array.\n"
		"2. If no suitable assets are available, set bSpawnActors to false.\n"
       
        "5a. SPAWNING: Each spawned actor must have unique ObjectName\n"
        "\n"
        "5b. PARTICLES: If the theme implies weather/magic (Rain, Fire, Snow), you MUST pick a particle from the list.\n"
		"   If no suitable particle exists, leave empty \"\".\n"
        "6. LOCATIONS: Use these semantic patterns for varied spatial distribution:\n"
        "   NAMED ZONES (use these for scene building):\n"
        "   - CENTER: Arena center (use for 1-3 key props)\n"
        "   - BACKGROUND: Far from camera (use for 3-6 walls/decorations)\n"
        "   - FOREGROUND: Close to camera (use for 1-3 interactive props)\n"
        "   - OVERHEAD: Aerial zone (use for particles/ceiling objects)\n"
        "   - LEFT_SIDE: Left arena (use for 3-5 props)\n"
        "   - RIGHT_SIDE: Right arena (use for 3-5 props)\n"
        "   - LEFT_CORNER, RIGHT_CORNER: Arena edges\n"
        "   \n"
        "   PLAYER-RELATIVE (use sparingly, max 2-3 spawns):\n"
        "   - PLAYER_FRONT, PLAYER_BACK, PLAYER_LEFT, PLAYER_RIGHT\n"
        "   \n"
        "   DYNAMIC QUERIES (for gameplay-driven spawns):\n"
        "   - CLOSEST:<Tag>: Near player-facing side of tagged actor\n"
        "   - NEAR:<Tag>: Random offset from tagged actor\n"
        "   \n"
        "   EXPLICIT (when exact control needed):\n"
        "   - CUSTOM:[X,Y,Z]: Example CUSTOM:[500,-300,100]\n"
        "\n"
        "7. RETURN ONLY VALID JSON - no markdown, code blocks, or explanations\n"
        "\n"
        "8. SPAWN DISTRIBUTION STRATEGY:\n"
        "   QUANTITY (match user intent):\n"
        "   - Explicit count request (add 3 walls) → Spawn that exact number\n"
        "   - Minimalist/simple/few → 5-10 spawns\n"
        "   - Standard theme/scene → 12-18 spawns\n"
        "   - Fill/crowded/detailed → 20-30 spawns\n"
        "   \n"
        "   SPATIAL COVERAGE (mandatory):\n"
        "   - Distribute across MULTIPLE zones (BACKGROUND, LEFT_SIDE, RIGHT_SIDE, CENTER)\n"
        "   - NEVER concentrate >30%% of spawns in PLAYER-RELATIVE positions\n"
        "   - Each major zone (BACKGROUND, LEFT, RIGHT, CENTER) should have 2+ spawns\n"
        "   \n"
        "   MESH VARIETY:\n"
        "   - Use 4+ different meshes\n"
        "   - No mesh should appear more than 25%% of total spawns\n"
        "   - Mix sizes: large (walls), medium (furniture), small (decorations)\n"
        "   \n"
        "   SPACING:\n"
        "   - LocationOffset: Vary between ±100 and ±500 based on object size\n"
        "   - Large objects (walls): ±300-600\n"
        "   - Small objects (props): ±100-300\n"
        "\n"

        "\n"
		"9. SCALE GUIDELINES:\n"
		"   - IMPORTANT: JSON Format Rule -> NO LEADING ZEROS for integers.\n"
		"   - Small props (barrels, baskets): [0.5-1.5, 0.5-1.5, 0.5-1.5]\n"
		"   - Medium props (benches, tables): [1.0-2.0, 1.0-2.0, 1.0-2.0]\n"
		"   - Large objects (walls, buildings): [1.0-3.0, 1.0-3.0, 1.0-3.0]\n"
		"   - NEVER use scale >4.0 - objects become too large\n"
		"   - Default scale: [1, 1, 1] if unsure\n"
		"\n"
        "10.ALWAYS set bModifyProps to true if the theme requires a material change (e.g. converting concrete walls to wood)."
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
        "      \"Rotation\": [Pitch, Yaw, Roll],\n"
        "      \"Scale\": [X, Y, Z],\n"
        "      \"Tag\": \"optional_tag\",\n"
        "      \"ClearanceRadius\": 150\n"
        "    }\n"
        "  ]\n"
        "  \"ParticleSpawn\": [\n"
		"    {\n"
		"      \"AssetPath\": \"exact_particle_name\",\n"
		"      \"ObjectName\": \"unique_instance_name\",\n"
		"      \"LocationName\": \"SEMANTIC_LOCATION\",\n"
		"      \"LocationOffset\": [0, 0, 0],\n"
		"      \"Rotation\": [Pitch, Yaw, Roll],\n"
		"      \"Scale\": [X, Y, Z],\n"
		"      \"Tag\": \"optional_tag\",\n"
		"      \"ClearanceRadius\": 150\n"
		"    }\n"
		"  ]\n"
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

	Request->OnProcessRequestComplete().BindUObject(this, &UMeshResolverLLM::OnPlanReceived);
	
	Request->ProcessRequest();
	
}

void UMeshResolverLLM::OnPlanReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful&&!Response.IsValid())
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
		FString Choices="NONE";
		OnMeshPlanReady.Broadcast(LLMResponseString,Choices);
	}

	
}
