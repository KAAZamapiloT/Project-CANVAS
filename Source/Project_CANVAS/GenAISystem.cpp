// Fill out your copyright notice in the Description page of Project Settings.


#include "GenAISystem.h"
#include"SceneStateTracker.h"
#include "JsonParser.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include"Kismet/GameplayStatics.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "API_KEY.h"
#include "AssetIndexer.h"
#include "SceneHistoryManager.h"


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

    // 3. Fetch the asset list
    TArray<FString> AvailableTextures = GameInstance->AssetIndexer->GetDiscoveredTextureNames();
    TArray<FString> AvailableTags = GameInstance->TargetableActorTags;
    TArray<FString> AvailablePPMs = GameInstance->TargetablePostProcessMaterials;

    // Store user prompt
    LastUserPrompt = UserPrompt;

    // Construct master prompt
    FString MasterPrompt = ConstructMasterPrompt(UserPrompt, AvailableTextures, AvailableTags, AvailablePPMs, HistoryManager);

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
        "\"model\":\"openai/gpt-oss-120b\","
        "\"messages\":["
            "{\"role\":\"system\",\"content\":\"You are a JSON generator for a 3D scene builder. Only respond with valid JSON, no markdown, no code blocks.\"},"
            "{\"role\":\"user\",\"content\":\"%s\"}"
        "],"
        "\"temperature\":0.2,"
        "\"max_tokens\":2000"
        "}"
    ), *MasterPrompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

    // === CHANGES END HERE ===

    UE_LOG(LogTemp, Display, TEXT("GenAI: Payload length: %d chars"), Payload.Len());
    Request->SetContentAsString(Payload);

    // Bind callback
    Request->OnProcessRequestComplete().BindUObject(this, &UGenAISystem::OnGroqResponseReceived);

    // Send request
    Request->ProcessRequest();
	
}

void UGenAISystem::OnGroqResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
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
    const TArray<FString>& AvailableTextures,
    const TArray<FString>& AvailableTags, 
    const TArray<FString>& AvailablePPMs,
    USceneHistoryManager* HistoryManager)
{
	// === STEP 1: GROUP TEXTURES INTO MATERIAL SETS ===
	TMap<FString, bool> MaterialBaseNames;  // Use as a set
    
	for (const FString& Texture : AvailableTextures)
	{
		FString BaseName = ExtractMaterialBaseName(Texture);
		if (!BaseName.IsEmpty())
		{
			MaterialBaseNames.Add(BaseName, true);
		}
	}
    
	// Convert to array
	TArray<FString> MaterialList;
	MaterialBaseNames.GetKeys(MaterialList);
    
	UE_LOG(LogTemp, Display, TEXT("GenAI: Reduced %d textures → %d materials"), 
		AvailableTextures.Num(), MaterialList.Num());
    
	UE_LOG(LogTemp, Display, TEXT("GenAI: Reduced %d textures → %d materials"), 
		AvailableTextures.Num(), MaterialList.Num());
	
	// === STEP 2: BUILD STRINGS ===
	FString MaterialListString = FString::Join(MaterialList, TEXT("\", \""));
	FString TagListString = FString::Join(AvailableTags, TEXT("\", \""));
	FString PPMListString = FString::Join(AvailablePPMs, TEXT("\", \""));
	FString TexturesListString = FString::Join(AvailableTextures, TEXT("\", \""));
    // === GET HISTORY CONTEXT (if available) ===
    FString HistoryContext = TEXT("");
    if (HistoryManager)
    {
        HistoryContext = HistoryManager->GetLLMContextString();
        UE_LOG(LogTemp, Display, TEXT("GenAISystem: Including history context in prompt"));
    }
    
    // === STEP 4: CONSTRUCT PROMPT ===
    return FString::Printf(TEXT(
        "You are an expert game environment designer. Generate a JSON scene plan for this request:\n"
        "USER REQUEST: \"%s\"\n\n"
        "%s"  // History context
        
       
        "=== CRITICAL MATERIAL RULES ===\n"
	   "MATERIALS: Each material name represents a COMPLETE PBR texture set.\n"
	   "- When you specify 'wood_floor', the system will automatically load:\n"
	   "  * wood_floor_diff_2k (base color)\n"
	   "  * wood_floor_rough_2k (roughness)\n"
	   "  * wood_floor_nor_gl_2k (normal map)\n"
	   "  * wood_floor_metal_2k (metallic)\n"
	   "  * wood_floor_ao_2k (ambient occlusion)\n"
	   "- In your JSON, ONLY use the base material name (e.g., 'wood_floor')\n"
	   "- DO NOT specify individual texture map names (_diff, _rough, etc.)\n\n"
        "=== CRITICAL TAG RULES ===\n"
        "VALID TAGS: [\"%s\"]\n"
        "- TagName = Actor tag from this list (identifies WHICH object to modify)\n"
        "- Texture paths = Material names from MATERIAL LIST below\n"
        "- NEVER mix tags and materials!\n\n"
        
        "=== JSON FORMAT ===\n"
        "{\n"
        "  \"ThemeName\": \"Your Theme Name\",\n"
        "  \"bModifyEnvironment\": true/false,\n"
        "  \"bModifyProps\": true/false,\n"
        "  \"TargetPropTags\": [\"Ground.Floor\", \"Background.Wall\"],\n"
        "  \"Environment\": {\n"
        "    \"FogDensity\": 0.1,\n"
        "    \"FogColor\": [R, G, B],\n"
        "    \"Lighting\": {\n"
        "      \"SunColor\": [R, G, B],\n"
        "      \"SunIntensity\": 10.0,\n"
        "      \"SunPitch\": -45.0,\n"
        "      \"SkyLightColor\": [R, G, B],\n"
        "      \"SkyLightIntensity\": 1.0\n"
        "    }\n"
        "  },\n"
        "  \"Props\": [\n"
        "    {\n"
        "      \"TagName\": \"Ground.Floor\",\n"
        "      \"PropColor\": [R, G, B],\n"
        "      \"Texture\": {\n"
        "        \"BaseColorPath\": \"black_painted_planks\",\n"
        "        \"NormalPath\": \"\",\n"
        "        \"RoughnessPath\": \"\",\n"
        "        \"MetallicPath\": \"\",\n"
        "        \"AOPath\": \"\"\n"
        "      },\n"
        "      \"ParticleEffects\": \"\"\n"
        "    }\n"
        "  ]\n"
        "}\n\n"
        
        "=== AVAILABLE ACTOR TAGS ===\n"
        "[\"%s\"]\n\n"
        
        "=== AVAILABLE MATERIALS ===\n"
        "[\"%s\"]\n\n"
        
        "=== AVAILABLE POST-PROCESS MATERIALS ===\n"
        "[\"%s\"]\n\n"
        
        "Generate ONLY valid JSON (no markdown, no code blocks):"
    ),
    *UserPrompt,
    *HistoryContext,
    *TagListString,
    *TagListString,
    *MaterialListString,  // ✅ NOW USING FILTERED MATERIAL LIST
    *PPMListString);
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
//A LOCAL OLLAMA FUNCTION CAN BE USED IF WE HAVE MORE RAM AVALIBLE BUT WE DONT


/**
 *
 *
*"=== CRITICAL MATERIAL RULES ===\n"
	   "MATERIALS: Each material name represents a COMPLETE PBR texture set.\n"
	   "- When you specify 'wood_floor', the system will automatically load:\n"
	   "  * wood_floor_diff_2k (base color)\n"
	   "  * wood_floor_rough_2k (roughness)\n"
	   "  * wood_floor_nor_gl_2k (normal map)\n"
	   "  * wood_floor_metal_2k (metallic)\n"
	   "  * wood_floor_ao_2k (ambient occlusion)\n"
	   "- In your JSON, ONLY use the base material name (e.g., 'wood_floor')\n"
	   "- DO NOT specify individual texture map names (_diff, _rough, etc.)\n\n"
 *
 *
 *void UGenAISystem::OnOllamaResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
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

	
}**/