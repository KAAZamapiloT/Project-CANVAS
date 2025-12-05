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
    
    UE_LOG(LogTemp, Display, TEXT("✅ LocationResolver: %s @ %s"), 
        *ModelName, *Endpoint);
}

FTransform ULocationResolverLLM::ResolveLocation(
    const FString& LocationName,
    const FString& SceneContext,
    const FSpawnRequest* SpawnRequest)
{
    if (!IsEnabled())
    {
        return FTransform::Identity;
    }

    // Check cache
    FString Key = LocationName.ToUpper();
    if (Cache.Contains(Key))
    {
        return Cache[Key];
    }

    // Build prompt
    FString Prompt = BuildPrompt(LocationName, SceneContext, SpawnRequest);

    // Call appropriate resolver
    FString Response;
    if (APIKey.IsEmpty())
    {
        Response = ResolveUsingLocal(Prompt);  // No auth
    }
    else
    {
        Response = ResolveUsingRemote(Prompt);  // With auth
    }

    if (Response.IsEmpty())
    {
        return FTransform::Identity;
    }

    // Parse
    FTransform Result;
    if (!ParseTransform(Response, Result))
    {
        return FTransform::Identity;
    }

    // Cache
    Cache.Add(Key, Result);

    UE_LOG(LogTemp, Display, TEXT("✅ LLM: '%s' → [%d, %d, %d]"),
        *LocationName,
        (int32)Result.GetLocation().X,
        (int32)Result.GetLocation().Y,
        (int32)Result.GetLocation().Z);

    return Result;
}

// ========================================
// REMOTE (with API key)
// ========================================
FString ULocationResolverLLM::ResolveUsingRemote(const FString& Prompt)
{
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *APIKey));  // ✅ Auth header

    // Build payload
    TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject());
    Payload->SetStringField(TEXT("model"), ModelName);
    Payload->SetNumberField(TEXT("max_tokens"), 100);
    Payload->SetNumberField(TEXT("temperature"), 0.1f);

    TArray<TSharedPtr<FJsonValue>> Messages;
    TSharedPtr<FJsonObject> Msg = MakeShareable(new FJsonObject());
    Msg->SetStringField(TEXT("role"), TEXT("user"));
    Msg->SetStringField(TEXT("content"), Prompt);
    Messages.Add(MakeShareable(new FJsonValueObject(Msg)));
    Payload->SetArrayField(TEXT("messages"), Messages);

    FString PayloadStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadStr);
    FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
    Request->SetContentAsString(PayloadStr);

    Request->ProcessRequest();

    // Wait
    float Elapsed = 0.0f;
    while (Request->GetStatus() == EHttpRequestStatus::Processing && Elapsed < Timeout)
    {
        FPlatformProcess::Sleep(0.05f);
        Elapsed += 0.05f;
    }

    if (Request->GetStatus() != EHttpRequestStatus::Succeeded)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Remote LLM failed"));
        return TEXT("");
    }

    // Parse response
    FString Body = Request->GetResponse()->GetContentAsString();
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObj))
    {
        return TEXT("");
    }

    const TArray<TSharedPtr<FJsonValue>>* Choices;
    if (!JsonObj->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
    {
        return TEXT("");
    }

    return (*Choices)[0]->AsObject()->GetObjectField(TEXT("message"))->GetStringField(TEXT("content"));
}

// ========================================
// LOCAL (no API key)
// ========================================
FString ULocationResolverLLM::ResolveUsingLocal(const FString& Prompt)
{
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    // ✅ NO Authorization header

    // Build payload (same as remote)
    TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject());
    Payload->SetStringField(TEXT("model"), ModelName);
    Payload->SetNumberField(TEXT("max_tokens"), 100);
    Payload->SetNumberField(TEXT("temperature"), 0.1f);

    TArray<TSharedPtr<FJsonValue>> Messages;
    TSharedPtr<FJsonObject> Msg = MakeShareable(new FJsonObject());
    Msg->SetStringField(TEXT("role"), TEXT("user"));
    Msg->SetStringField(TEXT("content"), Prompt);
    Messages.Add(MakeShareable(new FJsonValueObject(Msg)));
    Payload->SetArrayField(TEXT("messages"), Messages);

    FString PayloadStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadStr);
    FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
    Request->SetContentAsString(PayloadStr);

    Request->ProcessRequest();

    // Wait
    float Elapsed = 0.0f;
    while (Request->GetStatus() == EHttpRequestStatus::Processing && Elapsed < Timeout)
    {
        FPlatformProcess::Sleep(0.05f);
        Elapsed += 0.05f;
    }

    if (Request->GetStatus() != EHttpRequestStatus::Succeeded)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Local LLM failed"));
        return TEXT("");
    }

    // Parse response (same as remote)
    FString Body = Request->GetResponse()->GetContentAsString();
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObj))
    {
        return TEXT("");
    }

    const TArray<TSharedPtr<FJsonValue>>* Choices;
    if (!JsonObj->TryGetArrayField(TEXT("choices"), Choices) || Choices->Num() == 0)
    {
        return TEXT("");
    }

    return (*Choices)[0]->AsObject()->GetObjectField(TEXT("message"))->GetStringField(TEXT("content"));
}

// ========================================
// HELPERS
// ========================================
FString ULocationResolverLLM::BuildPrompt(
    const FString& LocationName,
    const FString& SceneContext,
    const FSpawnRequest* SpawnRequest)
{
    FString Extra;
    if (SpawnRequest)
    {
        Extra = FString::Printf(TEXT("Object: %s\n"), *SpawnRequest->ObjectName);
    }

    return FString::Printf(
        TEXT("2.5D fighting arena. Resolve location to integers.\n"
             "LOCATION: \"%s\"\n%s\n%s\n"
             "OUTPUT JSON: {\"x\": <int>, \"y\": <int>, \"z\": <int>, \"yaw\": <int 0-360>}"),
        *LocationName, *Extra, *SceneContext
    );
}

bool ULocationResolverLLM::ParseTransform(const FString& JsonResponse, FTransform& OutTransform)
{
    TSharedPtr<FJsonObject> JsonObj;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResponse);
    
    if (!FJsonSerializer::Deserialize(Reader, JsonObj))
    {
        return false;
    }

    int32 X = (int32)JsonObj->GetNumberField(TEXT("x"));
    int32 Y = (int32)JsonObj->GetNumberField(TEXT("y"));
    int32 Z = (int32)JsonObj->GetNumberField(TEXT("z"));
    int32 Yaw = (int32)JsonObj->GetNumberField(TEXT("yaw"));

    OutTransform.SetLocation(FVector(X, Y, Z));
    OutTransform.SetRotation(FQuat(FRotator(0, Yaw, 0)));

    return true;
}
// LocationResolverLLM.cpp

void ULocationResolverLLM::ResolveBatchLocationsAsync(
    const TArray<FSpawnRequest>& Requests, 
    const FString& SceneContext, 
    FOnBatchLocationsResolved Callback)
{
    // 1. Build the Batch Prompt
    FString Prompt = BuildBatchPrompt(Requests, SceneContext);

    // 2. Setup Request
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    if (!APIKey.IsEmpty()) Request->SetHeader("Authorization", FString::Printf(TEXT("Bearer %s"), *APIKey));

    // Build Payload (Groq/OpenAI format)
    TSharedPtr<FJsonObject> Payload = MakeShareable(new FJsonObject());
    Payload->SetStringField("model", ModelName);
    Payload->SetNumberField("temperature", 0.2); // Low temp = better math
    
    TArray<TSharedPtr<FJsonValue>> Messages;
    TSharedPtr<FJsonObject> Msg = MakeShareable(new FJsonObject());
    Msg->SetStringField("role", "user");
    Msg->SetStringField("content", Prompt);
    Messages.Add(MakeShareable(new FJsonValueObject(Msg)));
    
    Payload->SetArrayField("messages", Messages);

    FString PayloadStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&PayloadStr);
    FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);
    Request->SetContentAsString(PayloadStr);

    // 3. ASYNC CALLBACK (The magic part)
   Request->OnProcessRequestComplete().BindLambda(
         [this, Callback](FHttpRequestPtr Req, FHttpResponsePtr Res, bool bConnected)
        {
            FLocationMap Results; // ✅ Use the typedef
            
            if (bConnected && Res.IsValid() && Res->GetResponseCode() == 200)
            {
                TSharedPtr<FJsonObject> JsonObj;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
                
                if (FJsonSerializer::Deserialize(Reader, JsonObj))
                {
                    // Extract content string from OpenAI/Groq format
                    const TArray<TSharedPtr<FJsonValue>>* Choices;
                    if (JsonObj->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
                    {
                        FString Content = (*Choices)[0]->AsObject()->GetObjectField(TEXT("message"))->GetStringField(TEXT("content"));
                        
                        // Clean markdown
                        Content = Content.Replace(TEXT("```json"), TEXT("")).Replace(TEXT("```"), TEXT("")).TrimStartAndEnd();
                        
                        // Parse inner JSON map
                        TSharedPtr<FJsonObject> MapObj;
                        TSharedRef<TJsonReader<>> MapReader = TJsonReaderFactory<>::Create(Content);
                        
                        if (FJsonSerializer::Deserialize(MapReader, MapObj))
                        {
                            for (auto& Elem : MapObj->Values)
                            {
                                TSharedPtr<FJsonObject> VecObj = Elem.Value->AsObject();
                                if (VecObj.IsValid())
                                {
                                    FResolutionResult Item; // ✅ Use Struct

                                    // 1. Parse Location
                                    Item.Location.X = VecObj->GetNumberField(TEXT("x"));
                                    Item.Location.Y = VecObj->GetNumberField(TEXT("y"));
                                    Item.Location.Z = VecObj->GetNumberField(TEXT("z"));
                                    
                                    // 2. Parse Rotation (Yaw) - Optional
                                    double Yaw = 0;
                                    VecObj->TryGetNumberField(TEXT("yaw"), Yaw);
                                    Item.RotationYaw = (float)Yaw;
                                    
                                    // 3. Parse Scale - Optional, default 1.0
                                    double Scale = 1.0;
                                    if (VecObj->TryGetNumberField(TEXT("scale"), Scale))
                                    {
                                        Item.Scale = (float)Scale;
                                    }

                                    Results.Add(Elem.Key, Item);
                                }
                            }
                        }
                        else
                        {
                            UE_LOG(LogTemp, Error, TEXT("❌ Failed to deserialize inner JSON map: %s"), *Content);
                        }
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("❌ LLM Request Failed. Status: %d"), Res.IsValid() ? Res->GetResponseCode() : 0);
            }
            
            // EXECUTE CALLBACK
             Callback.ExecuteIfBound(Results);
        });

    Request->ProcessRequest();
}
FString ULocationResolverLLM::BuildBatchPrompt(const TArray<FSpawnRequest>& Requests, const FString& SceneContext)
{
    FString ItemList = "";
    for (const FSpawnRequest& Req : Requests)
    {
        ItemList += FString::Printf(TEXT("- ID: \"%s\", Preference: \"%s\", Radius: %.0f\n"), 
            *Req.ObjectName, *Req.LocationName, Req.ClearanceRadius);
    }

    return FString::Printf(TEXT(
        "Layout Task. Assign valid world coordinates (x,y,z), rotation (yaw), and scale for objects.\n"
        "SCENE CONTEXT:\n%s\n\n"
        "OBJECTS TO PLACE:\n%s\n"
        "RULES:\n"
        "1. Respect Bounds. Avoid overlaps.\n"
        "2. Scale: 1.0 is standard. 0.5 is half size, 2.0 is double. Use context to decide size.\n"
        "3. RETURN ONLY VALID JSON. Format:\n"
        "{\n"
        "  \"ObjID\": { \"x\": 100, \"y\": 200, \"z\": 0, \"yaw\": 90, \"scale\": 1.0 },\n"
        "  \"ObjID_2\": { \"x\": -500, \"y\": 50, \"z\": 0, \"yaw\": 0, \"scale\": 0.5 }\n"
        "}"), 
        *SceneContext, *ItemList);
}