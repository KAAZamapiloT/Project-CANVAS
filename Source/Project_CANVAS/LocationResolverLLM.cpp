// LocationResolverLLM.cpp

#include "LocationResolverLLM.h"
#include "ScenePlan.h"
#include "HttpModule.h"
#include "Json.h"
#include "JsonUtilities.h"

void ULocationResolverLLM::Configure(const FString& InEndpoint, const FString& InAPIKey, const FString& InModelName)
{
    Endpoint = InEndpoint;
    APIKey = InAPIKey;
    ModelName = InModelName;
    
    UE_LOG(LogTemp, Display, TEXT("✅ LocationResolver Configured: %s"), *ModelName);
}

// ... [Keep ResolveLocation, BuildPrompt, ParseTransform the same as before] ...

// ========================================
// SINGLE REQUEST (REMOTE)
// ========================================
FString ULocationResolverLLM::ResolveUsingRemote(const FString& Prompt)
{
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    
    // Auth Header Logic
    bool bIsGemini = Endpoint.Contains("googleapis");
    if (!bIsGemini && !APIKey.IsEmpty()) 
    {
        Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *APIKey));
    }

    FString PayloadStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadStr);

    // --- PAYLOAD SWITCHING ---
    if (bIsGemini)
    {
        // Gemini Payload
        TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject());
        TArray<TSharedPtr<FJsonValue>> Contents;
        TSharedPtr<FJsonObject> ContentObj = MakeShareable(new FJsonObject());
        TArray<TSharedPtr<FJsonValue>> Parts;
        TSharedPtr<FJsonObject> PartObj = MakeShareable(new FJsonObject());
        
        PartObj->SetStringField(TEXT("text"), Prompt);
        Parts.Add(MakeShareable(new FJsonValueObject(PartObj)));
        ContentObj->SetArrayField(TEXT("parts"), Parts);
        Contents.Add(MakeShareable(new FJsonValueObject(ContentObj)));
        
        Payload->SetArrayField(TEXT("contents"), Contents);
        FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
    }
    else
    {
        // OpenAI/Groq Payload
        TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject());
        Payload->SetStringField(TEXT("model"), ModelName);
        Payload->SetNumberField(TEXT("temperature"), 0.1f);

        TArray<TSharedPtr<FJsonValue>> Messages;
        TSharedPtr<FJsonObject> Msg = MakeShareable(new FJsonObject());
        Msg->SetStringField(TEXT("role"), TEXT("user"));
        Msg->SetStringField(TEXT("content"), Prompt);
        Messages.Add(MakeShareable(new FJsonValueObject(Msg)));
        Payload->SetArrayField(TEXT("messages"), Messages);

        FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
    }

    Request->SetContentAsString(PayloadStr);
    Request->ProcessRequest();

    // Blocking wait (Only for single resolve)
    float Elapsed = 0.0f;
    while (Request->GetStatus() == EHttpRequestStatus::Processing && Elapsed < Timeout)
    {
        FPlatformProcess::Sleep(0.05f);
        Elapsed += 0.05f;
    }

    if (Request->GetStatus() != EHttpRequestStatus::Succeeded) return TEXT("");

    FString Body = Request->GetResponse()->GetContentAsString();
    
    // --- RESPONSE PARSING SWITCH ---
    return bIsGemini ? ParseGeminiResponse(Body) : ParseOpenAIResponse(Body);
}

// ========================================
// BATCH REQUEST (ASYNC)
// ========================================
void ULocationResolverLLM::ResolveBatchLocationsAsync(
    const TArray<FSpawnRequest>& Requests, 
    const FString& SceneContext, 
    FOnBatchLocationsResolved Callback)
{
    FString Prompt = BuildBatchPrompt(Requests, SceneContext);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    
    Request->SetURL(Endpoint);
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");

    bool bIsGemini = Endpoint.Contains("googleapis");
    FString FinalURL = Endpoint;
    
    if (bIsGemini)
    {
        // Gemini: Ensure Key is in URL parameter
        if (!APIKey.IsEmpty() && !FinalURL.Contains("key="))
        {
            FString Separator = FinalURL.Contains("?") ? "&" : "?";
            FinalURL += FString::Printf(TEXT("%skey=%s"), *Separator, *APIKey);
        }
    }
    else if (!APIKey.IsEmpty())
    {
        // OpenAI/Groq: Use Bearer Header
        Request->SetHeader("Authorization", FString::Printf(TEXT("Bearer %s"), *APIKey));
    }

    // --- PAYLOAD SWITCHING ---
    FString PayloadStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadStr);

    if (bIsGemini)
    {
        TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject());
        TArray<TSharedPtr<FJsonValue>> Contents;
        TSharedPtr<FJsonObject> ContentObj = MakeShareable(new FJsonObject());
        TArray<TSharedPtr<FJsonValue>> Parts;
        TSharedPtr<FJsonObject> PartObj = MakeShareable(new FJsonObject());
        
        PartObj->SetStringField(TEXT("text"), Prompt);
        Parts.Add(MakeShareable(new FJsonValueObject(PartObj)));
        ContentObj->SetArrayField(TEXT("parts"), Parts);
        Contents.Add(MakeShareable(new FJsonValueObject(ContentObj)));
        Payload->SetArrayField(TEXT("contents"), Contents);
        
        // Gemini Config
        TSharedPtr<FJsonObject> Config = MakeShareable(new FJsonObject());
        Config->SetNumberField(TEXT("temperature"), 0.2);
        Config->SetStringField(TEXT("responseMimeType"), TEXT("application/json"));
        Payload->SetObjectField(TEXT("generationConfig"), Config);

        FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
    }
    else
    {
        TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject());
        Payload->SetStringField("model", ModelName);
        Payload->SetNumberField("temperature", 0.2); 
        TArray<TSharedPtr<FJsonValue>> Messages;
        TSharedPtr<FJsonObject> Msg = MakeShareable(new FJsonObject());
        Msg->SetStringField("role", "user");
        Msg->SetStringField("content", Prompt);
        Messages.Add(MakeShareable(new FJsonValueObject(Msg)));
        Payload->SetArrayField("messages", Messages);
        FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
    }

    Request->SetContentAsString(PayloadStr);

    // --- ASYNC CALLBACK ---
    Request->OnProcessRequestComplete().BindLambda(
         [this, Callback, bIsGemini](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bConnected)
        {
            FLocationMap Results;
            
            if (bConnected && Res.IsValid() && Res->GetResponseCode() == 200)
            {
                FString ResponseBody = Res->GetContentAsString();
                FString Content;

                // 1. Extract Content
                if (bIsGemini)
                {
                    Content = ParseGeminiResponse(ResponseBody);
                }
                else
                {
                    Content = ParseOpenAIResponse(ResponseBody);
                }

                // 2. Clean Markdown
                Content = Content.Replace(TEXT("```json"), TEXT("")).Replace(TEXT("```"), TEXT("")).TrimStartAndEnd();
                
                // 3. Parse Coordinate Map
                TSharedPtr<FJsonObject> MapObj;
                TSharedRef<TJsonReader<>> MapReader = TJsonReaderFactory<>::Create(Content);
                
                if (FJsonSerializer::Deserialize(MapReader, MapObj))
                {
                    for (auto& Elem : MapObj->Values)
                    {
                        TSharedPtr<FJsonObject> VecObj = Elem.Value->AsObject();
                        if (VecObj.IsValid())
                        {
                            FResolutionResult Item;
                            Item.Location.X = VecObj->GetNumberField(TEXT("x"));
                            Item.Location.Y = VecObj->GetNumberField(TEXT("y"));
                            Item.Location.Z = VecObj->GetNumberField(TEXT("z"));
                            
                            double Yaw = 0;
                            VecObj->TryGetNumberField(TEXT("yaw"), Yaw);
                            Item.RotationYaw = (float)Yaw;
                            
                            double Scale = 1.0;
                            if (VecObj->TryGetNumberField(TEXT("scale"), Scale)) Item.Scale = (float)Scale;
                         
                            Results.Add(Elem.Key, Item);
                            // ✅ ADD THESE LINES to bridge to your Struct
 double PatternVal = 0;
 if (VecObj->TryGetNumberField(TEXT("pattern"), PatternVal)) {
     Item.PatternID = static_cast<int32>(PatternVal);
 }

 double RadiusVal = 0;
 if (VecObj->TryGetNumberField(TEXT("radius"), RadiusVal)) {
     Item.PatternRadius = static_cast<float>(RadiusVal);
 }

 double ScaleVal = 1.0;
 if (VecObj->TryGetNumberField(TEXT("scale"), ScaleVal)) {
     Item.Scale = static_cast<float>(ScaleVal);
 }
                        }
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("❌ Failed to parse location JSON: %s"), *Content);
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ Location Request Failed: %d"), Res.IsValid() ? Res->GetResponseCode() : 0);
            }
            
            Callback.ExecuteIfBound(Results);
        });

    Request->ProcessRequest();
}

// ========================================
// PARSING HELPERS
// ========================================
FString ULocationResolverLLM::ParseGeminiResponse(const FString& JsonResponse)
{
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);

    if (FJsonSerializer::Deserialize(Reader, JsonObj))
    {
        const TArray<TSharedPtr<FJsonValue>>* Candidates;
        if (JsonObj->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
        {
            TSharedPtr<FJsonObject> ContentObj = (*Candidates)[0]->AsObject()->GetObjectField(TEXT("content"));
            const TArray<TSharedPtr<FJsonValue>>* Parts;
            if (ContentObj->TryGetArrayField(TEXT("parts"), Parts) && Parts->Num() > 0)
            {
                return (*Parts)[0]->AsObject()->GetStringField(TEXT("text"));
            }
        }
    }
    return TEXT("");
}

FString ULocationResolverLLM::ParseOpenAIResponse(const FString& JsonResponse)
{
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);
    
    if (FJsonSerializer::Deserialize(Reader, JsonObj))
    {
        const TArray<TSharedPtr<FJsonValue>>* Choices;
        if (JsonObj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
        {
            return (*Choices)[0]->AsObject()->GetObjectField(TEXT("message"))->GetStringField(TEXT("content"));
        }
    }
    return TEXT("");
}

FString ULocationResolverLLM::BuildPrompt(const FString& LocationName, const FString& SceneContext, const FSpawnRequest* SpawnRequest)
{
    FString Extra = SpawnRequest ? FString::Printf(TEXT("Object: %s\n"), *SpawnRequest->ObjectName) : TEXT("");
    return FString::Printf(
        TEXT("2.5D fighting arena. Resolve to integers.\nLOCATION: \"%s\"\n%s\n%s\nOUTPUT JSON: {\"x\":0,\"y\":0,\"z\":0,\"yaw\":0}"),
        *LocationName, *Extra, *SceneContext);
}

FString ULocationResolverLLM::BuildBatchPrompt(const TArray<FSpawnRequest>& Requests, const FString& SceneContext)
{
    FString ItemList = "";
    for (const FSpawnRequest& Req : Requests)
    {
        ItemList += FString::Printf(TEXT("- ID: \"%s\", PreferredZone: \"%s\", MinClearance: %.0f\n"), 
            *Req.ObjectName, *Req.LocationName, Req.ClearanceRadius);
    }

    return FString::Printf(TEXT(
        "ROLE: Senior Spatial Engineer\n"
        "CONTEXT:\n%s\n\n"
        "TASK: Assign coordinates for these objects. Use 'pattern' to define group logic.\n"
        "OBJECTS TO PLACE:\n%s\n"
        "PATTERN DEFINITIONS:\n"
        "   - pattern 0: Single Item (Use x,y,z as exact location)\n"
        "   - pattern 1: Circle (Use x,y,z as center, 'radius' is circle size)\n"
        "   - pattern 2: Grid (Use x,y,z as center, 'radius' is spacing)\n"
        "   - pattern 3: Scatter (Use x,y,z as center, 'radius' is spread)\n\n"
        "RULES:\n"
        "1. Avoid the DANGER ZONE provided in the context.\n"
        "2. RETURN VALID JSON MAP:\n"
        "{\n"
        "  \"ObjID\": { \"x\": 100, \"y\": 200, \"z\": 0, \"yaw\": 90, \"pattern\": 0, \"radius\": 0 },\n"
        "  \"ObjID_2\": { \"x\": -500, \"y\": 500, \"z\": 0, \"yaw\": 0, \"pattern\": 1, \"radius\": 400 }\n"
        "}"), 
        *SceneContext, *ItemList);
}
bool ULocationResolverLLM::ParseTransform(const FString& JsonResponse, FTransform& OutTransform)
{
    // Minimal JSON parse logic for single Transform...
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);
    if (!FJsonSerializer::Deserialize(Reader, JsonObj)) return false;

    int32 X = (int32)JsonObj->GetNumberField(TEXT("x"));
    int32 Y = (int32)JsonObj->GetNumberField(TEXT("y"));
    int32 Z = (int32)JsonObj->GetNumberField(TEXT("z"));
    OutTransform.SetLocation(FVector(X, Y, Z));
    return true;
}