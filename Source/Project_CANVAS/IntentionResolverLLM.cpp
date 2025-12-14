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
        "TASK: Return a JSON Array of strings containing only the relevant asset categories from the list below that match the User Request.\n\n"
        
        "USER REQUEST: \"%s\"\n\n"
        
        "AVAILABLE CATEGORIES:\n"
        "[%s]\n\n"
        
        "RULES:\n"
        "1. Return ONLY valid JSON. No markdown, no explanations.\n"
        "2. Format: [\"Keyword1\", \"Keyword2\"]\n"
        "3. Select 5-15 most relevant items.\n"
        "4. Use exact spelling from the list.\n\n"
        
        "JSON RESPONSE:"
    ), *UserPrompt, *CategoryString);

    FString KEY = API_KEY::GetKey(); // Using Groq/Llama 3

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(TEXT("https://api.groq.com/openai/v1/chat/completions"));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *KEY));

    FString JsonPayload = FString::Printf(TEXT(
        "{"
        "\"model\": \"llama-3.3-70b-versatile\","
        "\"messages\": ["
        "  {\"role\": \"system\", \"content\": \"You are a JSON API. Output only a JSON array.\"},"
        "  {\"role\": \"user\", \"content\": \"%s\"}"
        "],"
        "\"temperature\": 0.2,"
        "\"max_tokens\": 2500"
        "}"
    ), *Prompt.Replace(TEXT("\""), TEXT("\\\"")).Replace(TEXT("\n"), TEXT("\\n")));

    Request->SetContentAsString(JsonPayload);
    Request->OnProcessRequestComplete().BindUObject(this, &UIntentionResolverLLM::OnResponseReceived);
    Request->ProcessRequest();
}

void UIntentionResolverLLM::OnResponseReceived(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    TArray<FString> Keywords;

    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Request failed"));
        OnIntentionReady.Broadcast(Keywords);
        return;
    }

   FString LLMResponseString;
        
        // [Existing Gemini/Groq parsing logic to get the raw text into LLMResponseString...]
        // (Assuming you have already extracted "content" or "text" into LLMResponseString)

        // === FIXED PARSING LOGIC ===
        int32 StartIdx = -1; 
        int32 EndIdx = -1;

        // Find coordinates of [ and ]
        // Note: FindChar returns bool, and outputs the index to the second argument
        bool bFoundStart = LLMResponseString.FindChar(TEXT('['), StartIdx);
        bool bFoundEnd = LLMResponseString.FindLastChar(TEXT(']'), EndIdx);

        if (bFoundStart && bFoundEnd && EndIdx > StartIdx)
        {
            // Extract the substring between [ and ] inclusive
            LLMResponseString = LLMResponseString.Mid(StartIdx, (EndIdx - StartIdx) + 1);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Could not find valid JSON array [...] in response."));
            // Optional: Broadcast empty/failed state here
            OnIntentionReady.Broadcast(TArray<FString>());
            return;
        }

        // === CLEANUP & PARSE ===
        // Remove newlines and potential comments
        FString CleanedJSON;
        TArray<FString> Lines;
        LLMResponseString.ParseIntoArrayLines(Lines);
        
        for (const FString& Line : Lines)
        {
            FString ProcessedLine = Line;
            // Strip // comments
            int32 CommentIdx;
            if (ProcessedLine.FindChar(TEXT('/'), CommentIdx)) // Simple check, careful with URLs
            {
               // Better comment stripper:
               int32 DoubleSlash = ProcessedLine.Find(TEXT("//"));
               if (DoubleSlash != INDEX_NONE)
               {
                   ProcessedLine = ProcessedLine.Left(DoubleSlash);
               }
            }
            CleanedJSON += ProcessedLine.TrimStartAndEnd();
        }

        // Parse Array
        TArray<TSharedPtr<FJsonValue>> JsonArray;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanedJSON);

        if (FJsonSerializer::Deserialize(Reader, JsonArray))
        {
            
            for (auto& Val : JsonArray)
            {
                Keywords.Add(Val->AsString());
            }
            
            // Success!
            OnIntentionReady.Broadcast(Keywords);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("IntentionResolver: Failed to deserialize JSON Array: %s"), *CleanedJSON);
            OnIntentionReady.Broadcast(TArray<FString>());
        }
}