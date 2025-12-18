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
    // 1. Declare output arrays
    TArray<FString> MeshKeywords;
    TArray<FString> TextureKeywords;
    TArray<FString> ParticleKeywords;

    // 2. HTTP Validation
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: HTTP Request failed"));
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
        return;
    }

    FString ResponseString = Response->GetContentAsString();
    FString LLMResponseString = "";

    // 3. Parse API Response (Groq/OpenAI/Gemini Standard)
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

    if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
    {
        // TRY GROQ / OPENAI FORMAT
        const TArray<TSharedPtr<FJsonValue>>* Choices;
        if (JsonObj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
        {
            TSharedPtr<FJsonObject> FirstChoice = (*Choices)[0]->AsObject();
            if (FirstChoice.IsValid())
            {
                TSharedPtr<FJsonObject> Message = FirstChoice->GetObjectField(TEXT("message"));
                if (Message.IsValid())
                {
                    LLMResponseString = Message->GetStringField(TEXT("content"));
                }
            }
        }
        // TRY GEMINI FORMAT
        else 
        {
            const TArray<TSharedPtr<FJsonValue>>* Candidates;
            if (JsonObj->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
            {
                TSharedPtr<FJsonObject> ContentObj = (*Candidates)[0]->AsObject()->GetObjectField(TEXT("content"));
                const TArray<TSharedPtr<FJsonValue>>* Parts;
                if (ContentObj->TryGetArrayField(TEXT("parts"), Parts) && Parts->Num() > 0)
                {
                    LLMResponseString = (*Parts)[0]->AsObject()->GetStringField(TEXT("text"));
                }
            }
        }
    }

    // 4. Validate Extraction
    if (LLMResponseString.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Failed to extract content from API Response"));
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
        return;
    }

    // 5. Clean JSON String (Find outer braces '{' and '}')
    int32 StartIdx = -1; 
    int32 EndIdx = -1;

    // [CHANGED] Search for '{' instead of '['
    bool bFoundStart = LLMResponseString.FindChar(TEXT('{'), StartIdx);
    bool bFoundEnd = LLMResponseString.FindLastChar(TEXT('}'), EndIdx);

    if (bFoundStart && bFoundEnd && EndIdx > StartIdx)
    {
        LLMResponseString = LLMResponseString.Mid(StartIdx, (EndIdx - StartIdx) + 1);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Could not find valid JSON Object in text: %s"), *LLMResponseString);
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
        return;
    }

    // 6. Deserialize the JSON Object
    TSharedPtr<FJsonObject> ResultObject;
    TSharedRef<TJsonReader<>> ObjectReader = TJsonReaderFactory<>::Create(LLMResponseString);

    if (FJsonSerializer::Deserialize(ObjectReader, ResultObject) && ResultObject.IsValid())
    {
        // Helper Lambda to safely extract arrays
        auto ExtractKeywords = [&](const FString& FieldName, TArray<FString>& TargetArray)
        {
            const TArray<TSharedPtr<FJsonValue>>* JsonArray;
            if (ResultObject->TryGetArrayField(FieldName, JsonArray))
            {
                for (const auto& Val : *JsonArray)
                {
                    FString Keyword = Val->AsString();
                    if (!Keyword.IsEmpty())
                    {
                        TargetArray.Add(Keyword);
                    }
                }
            }
        };

        // Extract using keys from your new prompt schema
        ExtractKeywords(TEXT("MeshKeywords"), MeshKeywords);
        ExtractKeywords(TEXT("TextureKeywords"), TextureKeywords);
        ExtractKeywords(TEXT("ParticleKeywords"), ParticleKeywords);
        
        UE_LOG(LogTemp, Log, TEXT("IntentionResolver: Success! Found Meshes: %d, Textures: %d, Particles: %d"), 
            MeshKeywords.Num(), TextureKeywords.Num(), ParticleKeywords.Num());

        // 7. Broadcast with 3 Params
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Failed to deserialize final JSON Object: %s"), *LLMResponseString);
        OnIntentionReady.Broadcast(MeshKeywords, TextureKeywords, ParticleKeywords);
    }
}