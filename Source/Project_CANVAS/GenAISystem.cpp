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
void UGenAISystem::RequestSceneChange(FString UserPrompt,UWorld* WorldContext)
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
	FString MasterPrompt = ConstructMasterPrompt(UserPrompt,AvailableTextures,AvailableTags,AvailablePPMs );

// creating a HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("http://localhost:11434/api/generate")); // Default Ollama URL
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// 4. Create the JSON payload for Ollama
	FString Payload = FString::Printf(TEXT(
		"{\"model\": \"phi3:mini\", \"prompt\": \"%s\", \"stream\": false}"
	), *MasterPrompt.Replace(TEXT("\""), TEXT("\\\""))); // Escape quotes in the prompt

	
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
        
        OnThemeDataReady.Broadcast(Plan);
        UE_LOG(LogTemp, Warning, TEXT("GENAI: Broadcast OnThemeDataReady"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse Ollama's MAIN response: %s"), *ResponseString);
    }

	
}

FString UGenAISystem::ConstructMasterPrompt(FString UserPrompt, const TArray<FString>& AvailableTextures, const TArray<FString>& AvailableTags, const TArray<FString>& AvailablePPMs)
{
    FString TextureListString = FString::Join(AvailableTextures, TEXT("\", \""));
    FString TagListString = FString::Join(AvailableTags, TEXT("\", \""));
    FString PPMListString = FString::Join(AvailablePPMs, TEXT("\", \""));
    
	return FString::Printf(TEXT(
	   "You are an expert game environment designer. Your task is to generate a JSON scene plan based on a user request. "
	   "User Request: \"%s\". "
       
	   "--- CRITICAL RULES ---"
	   "1. You MUST use textures ONLY from this list: [\"%s\"]. DO NOT invent texture names. "
	   "2. You MUST target actors using tags ONLY from this list: [\"%s\"]. "
	   "3. Your response MUST be ONLY the JSON object. Do not include markdown. "
	   "4. All colors MUST be arrays of 3 NUMBERS from 0-255. CORRECT: [0, 255, 0]. WRONG: [\"R\", \"G\", \"B\"]. "
	   "5. You MUST use EXACTLY this JSON structure. DO NOT change field names. "
       
	   "--- REQUIRED JSON FORMAT (COPY THIS EXACTLY) ---"
	   "{"
	   "  \"ThemeName\": \"<creative theme name>\","
	   "  \"Environment\": {"
	   "    \"FogDensity\": 0.1,"
	   "    \"FogColor\": [128, 200, 100],"
	   "    \"PostProcessingName\": \"PP_Default\""
	   "  },"
	   "  \"Props\": ["
	   "    {"
	   "      \"TagName\": \"<MUST be from tag list>\","
	   "      \"PropColor\": [255, 0, 0],"
	   "      \"Texture\": {"
	   "        \"BaseColorPath\": \"<texture from list>\","
	   "        \"NormalPath\": \"<texture from list>\","
	   "        \"RoughnessPath\": \"<texture from list>\","
	   "        \"MetallicPath\": \"<texture from list>\","
	   "        \"AOPath\": \"<texture from list>\""
	   "      },"
	   "      \"ParticleEffects\": \"\""
	   "    }"
	   "  ]"
	   "}"
       
	   "--- AVAILABLE RESOURCES ---"
	   "Textures: [\"%s\"] "
	   "Tags: [\"%s\"] "
	   "Post-Process: [\"%s\"] "
       
	   "Generate ONLY valid JSON matching the EXACT format above:"
	), 
	*UserPrompt,
	*TextureListString,
	*TagListString,
	*TextureListString,  // Repeat for resources section
	*TagListString,
	*PPMListString);
}


