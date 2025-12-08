// Fill out your copyright notice in the Description page of Project Settings.


#include "GenAISystem.h"
#include "SceneStateTracker.h"
#include "JsonParser.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "AssetIndexer.h"


void UGenAISystem::RequestSceneChange(FString UserPrompt,UWorld* WorldContext,USceneHistoryManager* HistoryManager)
{
	

	// 1. Get the GameInstance and AssetIndexer
    USceneStateTracker* GameInstance = Cast<USceneStateTracker>(UGameplayStatics::GetGameInstance(WorldContext));
    if (!GameInstance || !GameInstance->AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("GenAISystem: Cannot find GameInstance or AssetIndexer!"));
        return;
    }

    // 2. Check if the asset list is ready
    if (!GameInstance->AssetIndexer->IsScanComplete())
    {
        UE_LOG(LogTemp, Warning, TEXT("GenAISystem: Asset scan is not complete. Please wait."));
        return;
    }

    
    // Store user prompt
    LastUserPrompt = UserPrompt;

    // Construct master prompt
	// Inside RequestSceneChange()
	FString MasterPrompt = ConstructMasterPrompt(
		UserPrompt,
		GameInstance->AssetIndexer  // Pass directly
	);

    // === CHANGES START HERE ===
    
    // Get API key
	API_KEY APIKey;
    FString GroqAPIKey = APIKey.GetGroqKey();

    // Create HTTP request
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("https://api.groq.com/openai/v1/chat/completions")); // Changed URL
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *GroqAPIKey)); // Added auth

    // Build Groq JSON payload (OpenAI format)
    FString Payload = FString::Printf(TEXT(
        "{"
        "\"model\":\"openai/gpt-oss-20b\","
        "\"messages\":["
            "{\"role\":\"system\",\"content\":\"You are a JSON generator for a 3D scene builder. Only respond with valid JSON, no markdown, no code blocks.\"},"
            "{\"role\":\"user\",\"content\":\"%s\"}"
        "],"
        "\"temperature\":0.2,"
        "\"max_tokens\":7000"
        "}"
    ), *MasterPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

    // === CHANGES END HERE ===

    UE_LOG(LogTemp, Display, TEXT("GenAI: Payload length: %d chars"), Payload.Len());
    Request->SetContentAsString(Payload);

    // Bind callback
    Request->OnProcessRequestComplete().BindUObject(this, &UGenAISystem::OnLLMResponseReceived);

    // Send request
    Request->ProcessRequest();
	
}

void UGenAISystem::OnLLMResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Ollama request failed!"));
        return;
    }
    
	FString ResponseString = Response->GetContentAsString();
	UE_LOG(LogTemp, Warning, TEXT("GenAI: RAW GROQ RESPONSE:\n%s"), *ResponseString);
    
	TSharedPtr<FJsonObject> GroqJsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
    
	if (FJsonSerializer::Deserialize(Reader, GroqJsonObject) && GroqJsonObject.IsValid())
    {
		FString LlmResponseString;
        
		const TArray<TSharedPtr<FJsonValue>>* ChoicesArray;
		if (GroqJsonObject->TryGetArrayField(TEXT("choices"), ChoicesArray) && ChoicesArray->Num() > 0)
		{
			TSharedPtr<FJsonObject> FirstChoice = (*ChoicesArray)[0]->AsObject();
			TSharedPtr<FJsonObject> MessageObj = FirstChoice->GetObjectField(TEXT("message"));
			LlmResponseString = MessageObj->GetStringField(TEXT("content"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("GenAI: No choices in Groq response!"));
			return;
		}
		// === END OF GROQ-SPECIFIC CODE ===
        
		UE_LOG(LogTemp, Log, TEXT("GenAI: Extracted LLM response (raw): %s"), *LlmResponseString);
        
        // === STEP 1: Extract JSON between first { and last } ===
        int32 JsonStart = -1;
        int32 JsonEnd = -1;
        
        if (LlmResponseString.FindChar(TEXT('{'), JsonStart) && 
            LlmResponseString.FindLastChar(TEXT('}'), JsonEnd) && 
            JsonStart < JsonEnd)
        {
            LlmResponseString = LlmResponseString.Mid(JsonStart, (JsonEnd - JsonStart) + 1);
            UE_LOG(LogTemp, Log, TEXT("Step 1 - Extracted JSON block"));
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("JSON parsing error: Could not find valid braces."));
            LlmResponseString = "";
        }
        
        // === STEP 2: Remove JSON comments (// ...) ===
        if (!LlmResponseString.IsEmpty())
        {
            TArray<FString> Lines;
            LlmResponseString.ParseIntoArrayLines(Lines);
            FString CleanedJSON;
            
            for (const FString& Line : Lines)
            {
                FString ProcessedLine = Line;
                
                // Find and remove single-line comments
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
            
            LlmResponseString = CleanedJSON.TrimStartAndEnd();
            UE_LOG(LogTemp, Log, TEXT("Step 2 - Removed comments"));
        }
        
        // === STEP 3: Final log and parse ===
        UE_LOG(LogTemp, Warning, TEXT("Cleaned LLM response for parser:\n%s"), *LlmResponseString);
        
        // Parse the cleaned JSON
        FEnhancedScenePlan Plan = UJsonParser::CreatePlan(LlmResponseString);
        
        // Log the parsed plan data
        UE_LOG(LogTemp, Warning, TEXT("GENAI: Parsed Plan - Theme: %s, Prop Modifications: %d"),
            *Plan.ThemeName, Plan.Props.Num());
        
    	OnThemeDataReady.Broadcast(Plan, LastUserPrompt);
        UE_LOG(LogTemp, Warning, TEXT("GENAI: Broadcast OnThemeDataReady"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse Ollama's MAIN response: %s"), *ResponseString);
    }

	
}



FString UGenAISystem::ConstructMasterPrompt(
    FString UserPrompt,
    UAssetIndexer* AssetIndexer)
{
    if (!AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("GenAISystem: AssetIndexer is null"));
        return TEXT("");
    }

    // === STEP 1: GET ALL ASSETS FROM INDEXER ===
    TArray<FString> AvailableTextures = AssetIndexer->GetDiscoveredTextureNames();
    TArray<FString> ActorTags = AssetIndexer->GetDiscoveredActorTags();
    TArray<FString> AvailableMeshes = AssetIndexer->GetAllMeshNames();
    TArray<FString> AvailablePPMs = AssetIndexer->GetDiscoveredPostProcessNames();
	// In ConstructMasterPrompt:
	TArray<FString> AvailableParticles = AssetIndexer->GetDiscoveredParticleNames();
    // === STEP 2: GET MATERIAL BASE NAMES ===
    TArray<FString> MaterialBaseNames = AssetIndexer->GetMaterialBaseNames();
    UE_LOG(LogTemp, Display, TEXT("GenAI: Using %d materials from %d textures"), MaterialBaseNames.Num(), AvailableTextures.Num());
    UE_LOG(LogTemp, Display, TEXT("GenAI: Available meshes: %d"), AvailableMeshes.Num());


	TArray<FString> CleanParticleNames;
	for(const FString& Path : AvailableParticles) {
		CleanParticleNames.Add(FPaths::GetBaseFilename(Path));
	}
    // === STEP 3: BUILD MESH LIST STRING (first 30 meshes for context) ===
    FString MeshListString = TEXT("[");
    for (int32 i = 0; i < FMath::Min(30, AvailableMeshes.Num()); i++)
    {
        MeshListString += FString::Printf(TEXT("\"%s\""), *AvailableMeshes[i]);
        if (i < FMath::Min(30, AvailableMeshes.Num()) - 1) MeshListString += TEXT(", ");
    }
    if (AvailableMeshes.Num() > 30)
    {
        MeshListString += FString::Printf(TEXT(", ... %d more meshes]"), AvailableMeshes.Num() - 30);
    }
    else
    {
        MeshListString += TEXT("]");
    }

    // === STEP 4: CONVERT TO CSV STRINGS ===
    FString MaterialString = FString::Join(MaterialBaseNames, TEXT("\", \""));
    FString TagString = FString::Join(ActorTags, TEXT("\", \""));
    FString PPMString = FString::Join(AvailablePPMs, TEXT("\", \""));
	FString ParticleString = FString::Join(CleanParticleNames, TEXT("\", \""));
    // === STEP 5: BUILD COMPREHENSIVE PROMPT ===
    return FString::Printf(TEXT(
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
        
        "=== AVAILABLE ACTOR TAGS (for Props.TagName) ===\n"
        "Modify only actors with these tags:\n"
        "[\"%s\"]\n\n"
        
        "=== AVAILABLE MATERIALS (for Props.Texture.BaseColorPath) ===\n"
        "Use these material base names (system auto-loads PBR textures):\n"
        "[\"%s\"]\n\n"
        
        "=== AVAILABLE POST-PROCESS MATERIALS (for Environment.PostProcessingName) ===\n"
        "[\"%s\"]\n\n"
        "=== CRITICAL RULES ===\n"
        "1. ASSETS: For 'SpawnRequest.AssetPath', you may use EITHER a Static Mesh name OR a Particle Effect name from the lists above.\n"
        "2. If no suitable assets are available, set bSpawnActors to false.\n"
        "3. MATERIALS: Use ONLY base names (NOT full paths)\n"
        "   System handles loading: material_name_diff_2k, material_name_rough_2k, etc.\n"
        "4. TAGS: Use ONLY from AVAILABLE ACTOR TAGS for modification\n"
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
        
        "=== JSON SCHEMA ===\n"
        "{\n"
        "  \"ThemeName\": \"descriptive_name\",\n"
        "  \"bModifyEnvironment\": true/false,\n"
        "  \"bModifyProps\": true/false,\n"
        "  \"bSpawnActors\": true/false,\n"
        "  \"TargetPropTags\": [\"tag1\", \"tag2\"],\n"
        "  \"Environment\": {\n"
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
        "  },\n"
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
        "  \"SpawnRequest\": [\n"
        "    {\n"
        "      \"AssetPath\": \"exact_mesh_OR_particle_name\",\n"
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
    *MeshListString,
    *ParticleString,
    *TagString,
    *MaterialString,
    *PPMString);
}

FString UGenAISystem::ExtractMaterialBaseName(const FString& TextureName)
{
	FString BaseName = TextureName;
    
	// Remove all known PBR suffixes
	TArray<FString> Suffixes = {
		TEXT("_diff_2k"), TEXT("_diff_4k"), TEXT("_diff_1k"),
		TEXT("_rough_2k"), TEXT("_rough_4k"), TEXT("_rough_1k"),
		TEXT("_nor_gl_2k"), TEXT("_nor_gl_4k"), TEXT("_nor_gl_1k"),
		TEXT("_nor_dx_2k"), TEXT("_nor_dx_4k"), TEXT("_nor_dx_1k"),
		TEXT("_metal_2k"), TEXT("_metal_4k"), TEXT("_metal_1k"),
		TEXT("_arm_2k"), TEXT("_arm_4k"), TEXT("_arm_1k"),
		TEXT("_ao_2k"), TEXT("_ao_4k"), TEXT("_ao_1k"),
		TEXT("_disp_2k"), TEXT("_disp_4k"), TEXT("_disp_1k"),
		TEXT("_spec_ior_2k"), TEXT("_spec_ior_4k"), TEXT("_spec_ior_1k"),
		TEXT("_anisotropy_strength_2k"), TEXT("_anisotropy_strength_4k"),
		TEXT("_anisotropy_rotation_2k"), TEXT("_anisotropy_rotation_4k"),
		TEXT("_mask_2k"), TEXT("_mask_4k"), TEXT("_mask_1k"),
		TEXT("_N1"), TEXT("_N"), TEXT("_D1"), TEXT("_D"),  // For character textures like T_Manny
		TEXT("_MRA1"), TEXT("_MRA"), TEXT("_BN")
	};
    
	for (const FString& Suffix : Suffixes)
	{
		if (BaseName.EndsWith(Suffix))
		{
			BaseName.RemoveFromEnd(Suffix);
			return BaseName;  // Return immediately after first match
		}
	}
    
	// If no suffix matched, return original name
	return BaseName;
}


