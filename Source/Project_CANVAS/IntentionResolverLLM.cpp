#include "IntentionResolverLLM.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "API_KEY.h"

void UIntentionResolverLLM::RequestIntention(FString UserPrompt, const TArray<FString>& AllMeshes,const TArray<FString>& AllTextures
,const TArray<FString> AllParticles
)
{
    // Join categories for the prompt context
    FString MeshString = FString::Join(AllMeshes, TEXT(", "));
    FString TextureString = FString::Join(AllTextures, TEXT(", "));
    FString ParticleString = FString::Join(AllParticles, TEXT(", "));
    // STRICT JSON PROMPT
    FString Prompt = FString::Printf(TEXT(
        "ROLE: Semantic Asset Filter.\n"
        "TASK: Analyze the USER REQUEST and select the most relevant asset categories from the AVAILABLE LIST.\n"
        "OUTPUT FORMAT: A JSON object with a single key 'categories' containing the list of strings.\n\n"
        
        "USER REQUEST: \"%s\"\n\n"
        
        "AVAILABLE Meshes:\n"
        "[%s]\n\n"
        "AVAILABLE Particles:\n"
        "[%s]\n\n"
        "AVAILABLE Textures:\n"
        "[%s]\n\n"
        "RULES:\n"
        "1. Return ONLY valid JSON. No markdown, no explanations.\n"
        "2. If a term is not found, ignore it. DO NOT insert comments or explanations.\n"
        "3. Select most relevant items.\n"
        "4. Use exact spelling from the list.\n\n"
        
        "JSON RESPONSE:"
        "OUTPUT FORMAT:\n"
        "{\n"
        "  \"MeshKeywords\": [\"Keyword1\", \"Keyword2\"],\n"
        "  \"TextureKeywords\": [\"Material1\", \"Material2\"],\n"
        "  \"ParticleKeywords\": [\"Effect1\", \"Effect2\"]\n"
        "}"
    ), *UserPrompt, *MeshString, *TextureString, *ParticleString);

    FString KEY = API_KEY::GetKey(); // Using Groq/Llama 3

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("https://api.groq.com/openai/v1/chat/completions"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *KEY));

    // ✅ FIX: Added response_format and improved string escaping logic
    FString JsonPayload = FString::Printf(TEXT(
        "{"
        "\"model\": \"llama-3.3-70b-versatile\","
        "\"messages\": ["
        "  {\"role\": \"system\", \"content\": \"You are a strict JSON API. Output only valid JSON.\"},"
        "  {\"role\": \"user\", \"content\": \"%s\"}"
        "],"
        "\"temperature\": 0.1," // Low temp for strictness
        "\"response_format\": {\"type\": \"json_object\"}" // Force Valid JSON
        "}"
    ), *Prompt.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

    Request->SetContentAsString(JsonPayload);
    Request->OnProcessRequestComplete().BindUObject(this, &UIntentionResolverLLM::OnResponseReceived);
    Request->ProcessRequest();
}

void UIntentionResolverLLM::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    // 1. Prepare Output Arrays
    TArray<FString> MeshKeywords;
    TArray<FString> TextureKeywords;
    TArray<FString> ParticleKeywords;

    // 2. Network & Validity Checks
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: HTTP Request Failed or Response Invalid."));
        // Broadcast empty to unblock any waiting listeners
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
        return;
    }

    FString ResponseString = Response->GetContentAsString();
    FString ExtractedContent = "";

    // 3. Parse the Outer JSON (API Wrapper for Groq/OpenAI/Gemini)
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

    if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
    {
        // --- STRATEGY A: OpenAI / Groq Format ---
        const TArray<TSharedPtr<FJsonValue>>* Choices;
        if (JsonObj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
        {
            TSharedPtr<FJsonObject> FirstChoice = (*Choices)[0]->AsObject();
            if (FirstChoice.IsValid())
            {
                TSharedPtr<FJsonObject> Message = FirstChoice->GetObjectField(TEXT("message"));
                if (Message.IsValid())
                {
                    ExtractedContent = Message->GetStringField(TEXT("content"));
                }
            }
        }
        // --- STRATEGY B: Google Gemini Format ---
        else 
        {
            const TArray<TSharedPtr<FJsonValue>>* Candidates;
            if (JsonObj->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
            {
                TSharedPtr<FJsonObject> CandidateObj = (*Candidates)[0]->AsObject();
                if (CandidateObj.IsValid())
                {
                    TSharedPtr<FJsonObject> ContentObj = CandidateObj->GetObjectField(TEXT("content"));
                    if (ContentObj.IsValid())
                    {
                        const TArray<TSharedPtr<FJsonValue>>* Parts;
                        if (ContentObj->TryGetArrayField(TEXT("parts"), Parts) && Parts->Num() > 0)
                        {
                            ExtractedContent = (*Parts)[0]->AsObject()->GetStringField(TEXT("text"));
                        }
                    }
                }
            }
        }
    }

    // 4. Content Validation
    if (ExtractedContent.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Failed to extract valid content string from API response."));
        UE_LOG(LogTemp, Warning, TEXT("Raw Response: %s"), *ResponseString);
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
        return;
    }

    // 5. Clean Markdown & Extract Inner JSON
    // LLMs often wrap output in ```json ... ``` or add preamble text.
    // We strictly look for the first '{' and the last '}'.
    int32 StartIdx = -1; 
    int32 EndIdx = -1;

    bool bFoundStart = ExtractedContent.FindChar(TEXT('{'), StartIdx);
    bool bFoundEnd = ExtractedContent.FindLastChar(TEXT('}'), EndIdx);

    if (bFoundStart && bFoundEnd && EndIdx > StartIdx)
    {
        ExtractedContent = ExtractedContent.Mid(StartIdx, (EndIdx - StartIdx) + 1);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Content did not contain valid JSON brackets. Content: %s"), *ExtractedContent);
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
        return;
    }

    // 6. Parse Inner JSON (The actual Keywords)
    TSharedPtr<FJsonObject> ResultObject;
    TSharedRef<TJsonReader<>> InnerReader = TJsonReaderFactory<>::Create(ExtractedContent);

    if (FJsonSerializer::Deserialize(InnerReader, ResultObject) && ResultObject.IsValid())
    {
        // Helper Lambda: Extracts and Sanitizes (Trims whitespace)
        auto ExtractCleanKeywords = [&](const FString& JsonKey, TArray<FString>& OutArray)
        {
            const TArray<TSharedPtr<FJsonValue>>* JsonArray;
            if (ResultObject->TryGetArrayField(JsonKey, JsonArray))
            {
                for (const auto& Val : *JsonArray)
                {
                    FString RawStr = Val->AsString();
                    RawStr.TrimStartAndEndInline(); // Remove " space "
                    if (!RawStr.IsEmpty())
                    {
                        OutArray.Add(RawStr);
                    }
                }
            }
        };

        // Extract using the exact keys defined in your Prompt
        ExtractCleanKeywords(TEXT("MeshKeywords"), MeshKeywords);
        ExtractCleanKeywords(TEXT("TextureKeywords"), TextureKeywords);
        ExtractCleanKeywords(TEXT("ParticleKeywords"), ParticleKeywords);

        // Success Log
        UE_LOG(LogTemp, Log, TEXT("IntentionResolver: ✅ Parsed Successfully.")); 
        UE_LOG(LogTemp, Log, TEXT("   - Meshes: %d"), MeshKeywords.Num());
        UE_LOG(LogTemp, Log, TEXT("   - Textures: %d"), TextureKeywords.Num());
        UE_LOG(LogTemp, Log, TEXT("   - Particles: %d"), ParticleKeywords.Num());

        // 7. Broadcast Success
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Failed to deserialize Inner JSON. Bad format?"));
        UE_LOG(LogTemp, Warning, TEXT("Cleaned Content: %s"), *ExtractedContent);
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
    }
}