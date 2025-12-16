#include "IntentionResolverLLM.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "API_KEY.h"

void UIntentionResolverLLM::RequestIntention(FString UserPrompt, const TArray<FString>& AllCategories)
{
    // Join categories for the prompt context
    FString CategoryString = FString::Join(AllCategories, TEXT(", "));

    // STRICT JSON PROMPT
    FString Prompt = FString::Printf(TEXT(
        "ROLE: Semantic Asset Filter.\n"
        "TASK: Analyze the USER REQUEST and select the most relevant asset categories from the AVAILABLE LIST.\n"
        "OUTPUT FORMAT: A JSON object with a single key 'categories' containing the list of strings.\n\n"
        
        "USER REQUEST: \"%s\"\n\n"
        
        "AVAILABLE CATEGORIES:\n"
        "[%s]\n\n"
        
        "RULES:\n"
        "1. Return ONLY valid JSON. No markdown, no explanations.\n"
        "2. If a term is not found, ignore it. DO NOT insert comments or explanations.\n"
        "3. Select most relevant items.\n"
        "4. Use exact spelling from the list.\n\n"
        
        "JSON RESPONSE:"
    ), *UserPrompt, *CategoryString);

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
    // 1. Declare output array once
    TArray<FString> Keywords;

    // 2. HTTP Validation
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: HTTP Request failed"));
        OnIntentionReady.Broadcast(Keywords);
        return;
    }

    FString ResponseString = Response->GetContentAsString();
    FString LLMResponseString = "";

    // 3. Parse API Response (Groq/OpenAI/Gemini Standard)
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);

    if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
    {
        // TRY GROQ / OPENAI FORMAT ("choices" -> "message" -> "content")
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
        // TRY GEMINI FORMAT ("candidates" -> "content" -> "parts" -> "text")
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
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Failed to extract content from API Response: %s"), *ResponseString);
        OnIntentionReady.Broadcast(Keywords);
        return;
    }

    // 5. Extract JSON Array [...]
    // Even though we asked for an object {"categories": [...]}, this logic still works
    // because it finds the FIRST '[' (start of list) and LAST ']' (end of list).
    int32 StartIdx = -1; 
    int32 EndIdx = -1;

    bool bFoundStart = LLMResponseString.FindChar(TEXT('['), StartIdx);
    bool bFoundEnd = LLMResponseString.FindLastChar(TEXT(']'), EndIdx);

    if (bFoundStart && bFoundEnd && EndIdx > StartIdx)
    {
        // Slice the string to get just the array content
        LLMResponseString = LLMResponseString.Mid(StartIdx, (EndIdx - StartIdx) + 1);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Could not find valid JSON array in text: %s"), *LLMResponseString);
        OnIntentionReady.Broadcast(Keywords);
        return;
    }

    // 6. Deserialize the Array
    TArray<TSharedPtr<FJsonValue>> JsonArray;
    TSharedRef<TJsonReader<>> ArrayReader = TJsonReaderFactory<>::Create(LLMResponseString);

    if (FJsonSerializer::Deserialize(ArrayReader, JsonArray))
    {
        for (auto& Val : JsonArray)
        {
            Keywords.Add(Val->AsString());
        }
        
        UE_LOG(LogTemp, Log, TEXT("IntentionResolver: Success! Found %d keywords."), Keywords.Num());
        OnIntentionReady.Broadcast(Keywords);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Failed to deserialize final JSON array: %s"), *LLMResponseString);
        OnIntentionReady.Broadcast(Keywords);
    }
}
