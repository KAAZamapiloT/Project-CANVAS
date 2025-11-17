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
