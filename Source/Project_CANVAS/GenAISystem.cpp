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
		// You could broadcast a failure event to the UI here
		return;
	}
	// 3. Fetch the asset list
	TArray<FString> AvailableTextures = GameInstance->AssetIndexer->GetDiscoveredTextureNames();
	TArray<FString> AvailableTags = GameInstance->TargetableActorTags;
	TArray<FString> AvailablePPMs = GameInstance->TargetablePostProcessMaterials;
	// constructing master prompt for model
	LastUserPrompt = UserPrompt;
	FString MasterPrompt = ConstructMasterPrompt(UserPrompt,AvailableTextures,AvailableTags,AvailablePPMs );

// creating a HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://localhost:11434/api/generate")); // Default Ollama URL
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// 4. Create the JSON payload for Ollama
	FString EscapedPrompt = MasterPrompt
	.Replace(TEXT("\\"), TEXT("\\\\"))  // Escape backslashes first
	.Replace(TEXT("\""), TEXT("\\\""))  // Escape quotes
	.Replace(TEXT("\n"), TEXT("\\n"))   // Escape newlines
	.Replace(TEXT("\r"), TEXT("\\r"))   // Escape carriage returns
	.Replace(TEXT("\t"), TEXT("\\t"));  // Escape tabs

	FString Payload = FString::Printf(TEXT(
		"{\"model\": \"llama3\", \"prompt\": \"%s\", \"stream\": false}"
	), *EscapedPrompt);

	UE_LOG(LogTemp, Display, TEXT("GenAI: Payload length: %d chars"), Payload.Len());
	Request->SetContentAsString(Payload);

	
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

	
}

FString UGenAISystem::ConstructMasterPrompt(
    FString UserPrompt, 
    const TArray<FString>& AvailableTextures,
    const TArray<FString>& AvailableTags, 
    const TArray<FString>& AvailablePPMs,
    USceneHistoryManager* HistoryManager)
{
    FString TextureListString = FString::Join(AvailableTextures, TEXT("\", \""));
    FString TagListString = FString::Join(AvailableTags, TEXT("\", \""));
    FString PPMListString = FString::Join(AvailablePPMs, TEXT("\", \""));
    
    // === GET HISTORY CONTEXT (if available) ===
    FString HistoryContext = TEXT("");
    if (HistoryManager)
    {
        HistoryContext = HistoryManager->GetLLMContextString();
        UE_LOG(LogTemp, Display, TEXT("GenAISystem: Including history context in prompt"));
    }
    
    return FString::Printf(TEXT(
        "You are an expert game environment designer. Generate a JSON scene plan for this request:\n"
        "USER REQUEST: \"%s\"\n\n"
        
        "%s"  // History context

          
        "=== CRITICAL TAG RULES (MUST FOLLOW) ===\n"
		"YOU MUST USE TAGS FROM THIS EXACT LIST. DO NOT INVENT NEW TAGS!\n"
		"VALID TAGS: [\"%s\"]\n\n"
		
        "=== CRITICAL RULES ===\n"
        "1. TagName field = Actor tag from TAG LIST (identifies WHICH object to modify)\n"
        "2. Texture paths = Texture names from TEXTURE LIST (what material to apply)\n"
        "3. NEVER mix tags and textures! They are SEPARATE lists!\n"
        "4. Colors = [R, G, B] with numbers 0-255 only\n"
        "5. Response = ONLY valid JSON, no markdown, no code blocks\n"
        "6. bModifyEnvironment = true ONLY if user mentions fog/lighting/atmosphere/scene\n"
        "7. bModifyProps = true ONLY if user mentions objects/surfaces/walls/floor/ground/ or background\n"
        "8. TargetPropTags = list of tags user explicitly mentions\n\n"
        "9. TargetPropTags = list EVERY tag you use in Props array\n"  
		"10. If user says 'wall' or 'background', use Background.Wall tag\n"  
		"11. If user says 'floor' or 'ground', use Ground.Floor tag\n\n"
		"=== LIGHTING CONTROL ===\n"
		"You can control lighting in the Environment.Lighting section:\n"
		"- SunColor: [R, G, B] (0-255) for sun/moon color\n"
		"- SunIntensity: 1-20 (typical: 10 for day, 0.5 for night)\n"
		"- SunPitch: -90 to 0 degrees (-45 = afternoon, -10 = sunset)\n"
		"- SunYaw: 0-360 degrees (direction: 0=north, 180=south)\n"
		"- SkyLightColor: [R, G, B] for ambient light\n"
		"- SkyLightIntensity: 0.1-2.0 (typical: 1.0)\n"
		"- SunTemperature: 2000-10000 Kelvin (3000=warm sunset, 6500=daylight, 9000=cool)\n\n"
        
		"LIGHTING EXAMPLES:\n"
		"- Sunset: SunColor=[255,150,80], SunPitch=-10, SunIntensity=8\n"
		"- Noon: SunColor=[255,255,240], SunPitch=-80, SunIntensity=15\n"
		"- Night: SunColor=[50,70,120], SunPitch=-30, SunIntensity=0.2\n"
		"- Horror: SunColor=[100,100,120], SunIntensity=2, SkyLightIntensity=0.3\n\n"
        
		"=== EXAMPLE JSON (WITH LIGHTING) ===\n"
		"{\n"
		"  \"ThemeName\": \"Roman Sunset\",\n"
		"  \"bModifyEnvironment\": true,\n"
		"  \"Environment\": {\n"
		"    \"FogDensity\": 0.02,\n"
		"    \"FogColor\": [240, 200, 150],\n"
		"    \"Lighting\": {\n"
		"      \"SunColor\": [255, 180, 100],\n"
		"      \"SunIntensity\": 8.0,\n"
		"      \"SunPitch\": -15.0,\n"
		"      \"SunYaw\": 180.0,\n"
		"      \"SkyLightColor\": [200, 180, 150],\n"
		"      \"SkyLightIntensity\": 0.8\n"
		"    }\n"
		"  }\n"
		"}\n\n"
        
        "=== EXAMPLE (CORRECT FORMAT) ===\n"
        "{\n"
        "  \"ThemeName\": \"Dark Forest\",\n"
        "  \"bModifyEnvironment\": false,\n"
        "  \"bModifyProps\": true,\n"
        "  \"TargetPropTags\": [\"Background.Wall\"],\n"
        "  \"Environment\": {\n"
        "    \"FogDensity\": 0.1,\n"
        "    \"FogColor\": [128, 200, 100],\n"
        "    \"PostProcessingName\": \"PP_Default\"\n"
        "  },\n"
        "  \"Props\": [\n"
        "    {\n"
        "      \"TagName\": \"Background.Wall\",\n"
        "      \"PropColor\": [80, 60, 40],\n"
        "      \"Texture\": {\n"
        "        \"BaseColorPath\": \"wood_table_rough_2k\",\n"
        "        \"NormalPath\": \"wood_table_nor_gl_2k\",\n"
        "        \"RoughnessPath\": \"wood_table_rough_2k\",\n"
        "        \"MetallicPath\": \"\",\n"
        "        \"AOPath\": \"wood_table_ao_2k\"\n"
        "      },\n"
        "      \"ParticleEffects\": \"\"\n"
        "    }\n"
        "  ]\n"
        "}\n\n"
        
        "=== AVAILABLE ACTOR TAGS (use in TagName field) ===\n"
        "[\"%s\"]\n\n"
        
        "=== AVAILABLE TEXTURES (use in Texture fields) ===\n"
        "[\"%s\"]\n\n"
        
        "=== AVAILABLE POST-PROCESS MATERIALS ===\n"
        "[\"%s\"]\n\n"
        
        "Generate JSON response (no markdown):"
    ), 
    *UserPrompt,
    *HistoryContext,*TagListString,
    *TagListString,      // ✅ Tags section FIRST
    *TextureListString,  // ✅ Textures section SECOND
    *PPMListString);
}



