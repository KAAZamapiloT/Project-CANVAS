// Fill out your copyright notice in the Description page of Project Settings.


#include "JsonParser.h"

#include <rapidjson/reader.h>

#include "AudioMixerBlueprintLibrary.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "GeometryCollection/GeometryCollectionParticlesData.h"


bool UJsonParser::bScehmaValidation(FString& JsonContext)
{
    if (JsonContext.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Validation: JSON context is empty"));
        return false;
    }

    // Parse once
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContext);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Validation: Failed to deserialize JSON"));
        return false;
    }

    TArray<FString> Errors;
    TArray<FString> Warnings;
    

    // ThemeName (required, non-empty string)
    if (!Root->HasField(TEXT("ThemeName")))
    {
        Errors.Add(TEXT("$.ThemeName is required"));
    }
    else
    {
        FString Theme;
        if (!Root->TryGetStringField(TEXT("ThemeName"), Theme) || Theme.TrimStartAndEnd().IsEmpty())
        {
            Errors.Add(TEXT("$.ThemeName must be a non-empty string"));
        }
    }

    // Flags (optional booleans)
    auto ValidateBool = [&](const TCHAR* Key)
    {
        if (Root->HasField(Key) && Root->TryGetField(Key)->Type != EJson::Boolean)
        {
            Errors.Add(FString::Printf(TEXT("$.%s must be a boolean"), Key));
        }
    };
    ValidateBool(TEXT("bModifyEnvironment"));
    ValidateBool(TEXT("bModifyProps"));

    // Environment (optional object)
    if (Root->HasField(TEXT("Environment")))
    {
        const TSharedPtr<FJsonObject> Env = Root->GetObjectField(TEXT("Environment"));
        if (!Env.IsValid())
        {
            Errors.Add(TEXT("$.Environment must be an object"));
        }
        else
        {
            // FogDensity number in [0.0, 5.0]
            if (Env->HasField(TEXT("FogDensity")))
            {
                double D = 0.0;
                if (!Env->TryGetNumberField(TEXT("FogDensity"), D))
                {
                    Errors.Add(TEXT("$.Environment.FogDensity must be a number"));
                }
                else if (D < 0.0 || D > 5.0)
                {
                    Warnings.Add(TEXT("$.Environment.FogDensity clamped to [0.0, 5.0]"));
                }
            }

            // FogColor int[3] 0–255
            if (Env->HasField(TEXT("FogColor")))
            {
                const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
                if (!Env->TryGetArrayField(TEXT("FogColor"), Arr) || Arr->Num() != 3)
                {
                    Errors.Add(TEXT("$.Environment.FogColor must be an array of 3 integers [0..255]"));
                }
                else
                {
                    for (int i = 0; i < 3; ++i)
                    {
                        if ((*Arr)[i]->Type != EJson::Number)
                        {
                            Errors.Add(TEXT("$.Environment.FogColor elements must be integers"));
                            break;
                        }
                    }
                }
            }

            // Lighting
            if (Env->HasField(TEXT("Lighting")))
            {
                const TSharedPtr<FJsonObject> Light = Env->GetObjectField(TEXT("Lighting"));
                if (!Light.IsValid())
                {
                    Errors.Add(TEXT("$.Environment.Lighting must be an object"));
                }
                else
                {
                    auto ValidateRgb = [&](const TCHAR* KeyPath)
                    {
                        if (Light->HasField(KeyPath))
                        {
                            const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
                            if (!Light->TryGetArrayField(KeyPath, A) || A->Num() != 3)
                            {
                                Errors.Add(FString::Printf(TEXT("$.Environment.Lighting.%s must be [r,g,b] integers"), KeyPath));
                            }
                        }
                    };
                    ValidateRgb(TEXT("SunColor"));
                    ValidateRgb(TEXT("SkyLightColor"));

                    auto ValidateNumberIn = [&](const TCHAR* Key, double Min, double Max)
                    {
                        if (Light->HasField(Key))
                        {
                            double V = 0.0;
                            if (!Light->TryGetNumberField(Key, V))
                            {
                                Errors.Add(FString::Printf(TEXT("$.Environment.Lighting.%s must be a number"), Key));
                            }
                            else if (V < Min || V > Max)
                            {
                                Warnings.Add(FString::Printf(TEXT("$.Environment.Lighting.%s clamped to [%.2f, %.2f]"), Key, Min, Max));
                            }
                        }
                    };
                    ValidateNumberIn(TEXT("SunIntensity"), 0.0, 50.0);
                    ValidateNumberIn(TEXT("SkyLightIntensity"), 0.0, 10.0);
                    ValidateNumberIn(TEXT("SunPitch"), -90.0, 90.0);
                    ValidateNumberIn(TEXT("SunYaw"), -360.0, 360.0);
                }
            }
        }
    }

    // Props (optional array)
    if (Root->HasField(TEXT("Props")))
    {
        const TArray<TSharedPtr<FJsonValue>>* Props = nullptr;
        if (!Root->TryGetArrayField(TEXT("Props"), Props))
        {
            Errors.Add(TEXT("$.Props must be an array"));
        }
        else
        {
            for (int i = 0; i < Props->Num(); ++i)
            {
                if ((*Props)[i]->Type != EJson::Object)
                {
                    Errors.Add(FString::Printf(TEXT("$.Props[%d] must be an object"), i));
                    continue;
                }
                const TSharedPtr<FJsonObject> P = (*Props)[i]->AsObject();
                // TagName (required)
                if (!P->HasField(TEXT("TagName")))
                {
                    Errors.Add(FString::Printf(TEXT("$.Props[%d].TagName is required"), i));
                }
                else
                {
                    FString Tag;
                    if (!P->TryGetStringField(TEXT("TagName"), Tag) || Tag.TrimStartAndEnd().IsEmpty())
                    {
                        Errors.Add(FString::Printf(TEXT("$.Props[%d].TagName must be a non-empty string"), i));
                    }
                }

                // PropColor optional [r,g,b]
                if (P->HasField(TEXT("PropColor")))
                {
                    const TArray<TSharedPtr<FJsonValue>>* C = nullptr;
                    if (!P->TryGetArrayField(TEXT("PropColor"), C) || C->Num() != 3)
                    {
                        Errors.Add(FString::Printf(TEXT("$.Props[%d].PropColor must be [r,g,b] integers"), i));
                    }
                }

                // Texture (optional object)
                if (P->HasField(TEXT("Texture")))
                {
                    const TSharedPtr<FJsonObject> T = P->GetObjectField(TEXT("Texture"));
                    if (!T.IsValid())
                    {
                        Errors.Add(FString::Printf(TEXT("$.Props[%d].Texture must be an object"), i));
                    }
                    else
                    {
                        // BaseColorPath required if Texture present
                        if (!T->HasField(TEXT("BaseColorPath")))
                        {
                            Errors.Add(FString::Printf(TEXT("$.Props[%d].Texture.BaseColorPath is required"), i));
                        }
                        else
                        {
                            FString B;
                            if (!T->TryGetStringField(TEXT("BaseColorPath"), B) || B.TrimStartAndEnd().IsEmpty())
                            {
                                Errors.Add(FString::Printf(TEXT("$.Props[%d].Texture.BaseColorPath must be a non-empty string"), i));
                            }
                        }
                    }
                }
            }
        }
    }

    // Emit summary
    if (Errors.Num() > 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Validation failed: %d error(s), %d warning(s)"), Errors.Num(), Warnings.Num());
        for (const FString& E : Errors) UE_LOG(LogTemp, Error, TEXT("%s"), *E);
        for (const FString& W : Warnings) UE_LOG(LogTemp, Warning, TEXT("%s"), *W);
        return false;
    }
    for (const FString& W : Warnings) UE_LOG(LogTemp, Warning, TEXT("%s"), *W);
    return true;
}

FEnhancedScenePlan UJsonParser::CreatePlan(FString JsonContext)
{
    UE_LOG(LogTemp, Warning, TEXT("Parser received this string: ---%s---"), *JsonContext);
    FEnhancedScenePlan Plan;
    
    // Pointer that holds our parsed data
    TSharedPtr<FJsonObject> JsonObject;

    // JSON reader to read a raw string
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContext);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // Parse ThemeName at root level
        if (JsonObject->HasField(TEXT("ThemeName")))
        {
            Plan.ThemeName = JsonObject->GetStringField(TEXT("ThemeName"));
            UE_LOG(LogTemp, Display, TEXT("PARSER: ThemeName: %s"), *Plan.ThemeName);
        }

        // Parse Environment object
        if (JsonObject->HasField(TEXT("Environment")))
        {
            const TSharedPtr<FJsonObject>* EnvironmentObject;
            if (JsonObject->TryGetObjectField(TEXT("Environment"), EnvironmentObject))
            {
                ParseEnvironment(*EnvironmentObject, Plan.Environment);
            }
        }
        
        // Parse Props array
        if (JsonObject->HasField(TEXT("Props")))
        {
            const TArray<TSharedPtr<FJsonValue>>* PropsArray;
            if (JsonObject->TryGetArrayField(TEXT("Props"), PropsArray))
            {
                for (const TSharedPtr<FJsonValue>& PropValue : *PropsArray)
                {
                    const TSharedPtr<FJsonObject>* PropObject;
                    if (PropValue->TryGetObject(PropObject))
                    {
                        FPropsModification PropMod;
                        ParsePropModification(*PropObject, PropMod);
                        Plan.Props.Add(PropMod);
                    }
                }
                UE_LOG(LogTemp, Display, TEXT("PARSER: Parsed %d prop modifications"), Plan.Props.Num());
            }
        }
        if (JsonObject->HasField(TEXT("SpawnRequest")))
        {
            const TArray<TSharedPtr<FJsonValue>>* SpawnsArray; // This is the JSON array
    
            // Correct: Use TryGetArrayField for the array
            if (JsonObject->TryGetArrayField(TEXT("SpawnRequest"), SpawnsArray)) 
            {
                // Loop over the JSON array
                for (const TSharedPtr<FJsonValue>& SpawnValue : *SpawnsArray)
                {
                    // Get the object { ... } from the array
                    const TSharedPtr<FJsonObject>* SpawnObject;
                    if (SpawnValue->TryGetObject(SpawnObject))
                    {
                        // Create a new struct to hold the data
                        FSpawnRequest SpawnReq; 
                
                        // Parse the data *into* the new struct
                        ParseSpawnRequest(*SpawnObject, SpawnReq); // This is your helper function
                
                        // Add the single parsed request to your plan's TArray
                        // This assumes Plan.SpawnRequest is your TArray<FSpawnRequest>
                        Plan.SpawnRequest.Add(SpawnReq); 
                    }
                }
                UE_LOG(LogTemp, Display, TEXT("PARSER: Parsed %d new actors to spawn"), Plan.SpawnRequest.Num());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("PARSER: 'SpawnRequest' field is not a JSON array!"));
            }
        }
        // DELTA CHANGES ADDITIONS
        if (JsonObject->HasField(TEXT("bModifyEnvironment")))
        {
            Plan.bModifyEnvironment = JsonObject->GetBoolField(TEXT("bModifyEnvironment"));
        }
    
        if (JsonObject->HasField(TEXT("bModifyProps")))
        {
            Plan.bModifyProps = JsonObject->GetBoolField(TEXT("bModifyProps"));
        }

        if (JsonObject->HasField(TEXT("TargetPropTags")))
        {
            const TArray<TSharedPtr<FJsonValue>>* TagsArray;
            if (JsonObject->TryGetArrayField(TEXT("TargetPropTags"), TagsArray))
            {
                for (const TSharedPtr<FJsonValue>& TagValue : *TagsArray)
                {
                    Plan.TargetPropTags.Add(TagValue->AsString());
                }
            }
        }
        if (JsonObject->HasField(TEXT("ParticleSpawns")))
        {
            const TArray<TSharedPtr<FJsonValue>>* ParticleArray;
            if (JsonObject->TryGetArrayField(TEXT("ParticleSpawns"), ParticleArray))
            {
                for (const TSharedPtr<FJsonValue>& Val : *ParticleArray)
                {
                    const TSharedPtr<FJsonObject>* ParticleObj;
                    if (Val->TryGetObject(ParticleObj))
                    {
                        FSpawnRequest ParticleReq;
                        // Re-use ParseSpawnRequest because the JSON structure is identical
                        ParseSpawnRequest(*ParticleObj, ParticleReq);
                        Plan.ParticleSpawns.Add(ParticleReq);
                    }
                }
                UE_LOG(LogTemp, Display, TEXT("PARSER: Parsed %d particle spawns (ParticleSpawns)"), Plan.ParticleSpawns.Num());
            }
        }
        
        if (JsonObject->HasField(TEXT("bSpawnActors")))
        {
            Plan.bSpawnActors = JsonObject->GetBoolField(TEXT("bSpawnActors"));
            
        }
        
        return Plan;
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to parse JsonContext in CreatePlan()"));
        Plan.ThemeName = FString("Default");
        return Plan;
    }
}

void UJsonParser::ParseEnvironment(const TSharedPtr<FJsonObject>& JsonObject, FEnvironmentPlan& Environment)
{
    if (!JsonObject.IsValid())
    {
        return;
    }



    // Parse FogDensity
    if (JsonObject->HasField(TEXT("FogDensity")))
    {
        Environment.FogDensity = JsonObject->GetNumberField(TEXT("FogDensity"));
    }

    // Parse FogColor
    if (JsonObject->HasField(TEXT("FogColor")))
    {
        const TArray<TSharedPtr<FJsonValue>>* ColorArray;
        if (JsonObject->TryGetArrayField(TEXT("FogColor"), ColorArray) && ColorArray->Num() == 3)
        {
            int32 R = (*ColorArray)[0]->AsNumber();
            int32 G = (*ColorArray)[1]->AsNumber();
            int32 B = (*ColorArray)[2]->AsNumber();
            Environment.FogColor = FColor(R, G, B);
        }
    }

    // Parse PostProcessingName
    if (JsonObject->HasField(TEXT("PostProcessingName")))
    {
        Environment.PostProcessingName = JsonObject->GetStringField(TEXT("PostProcessingName"));
    }

    UE_LOG(LogTemp, Display, TEXT("PARSER: Environment - , FogDensity: %f"), 
            Environment.FogDensity);

    if (JsonObject->HasTypedField<EJson::Object>(TEXT("Lighting")))
    {
        TSharedPtr<FJsonObject> LightObj = JsonObject->GetObjectField(TEXT("Lighting"));
        
        // Parse sun color
        if (LightObj->HasField(TEXT("SunColor")))
        {
            const TArray<TSharedPtr<FJsonValue>>* ColorArray;
            if (LightObj->TryGetArrayField(TEXT("SunColor"), ColorArray) && ColorArray->Num() >= 3)
            {
                float R = (*ColorArray)[0]->AsNumber() / 255.0f;
                float G = (*ColorArray)[1]->AsNumber() / 255.0f;
                float B = (*ColorArray)[2]->AsNumber() / 255.0f;
                Environment.Lighting.SunColor = FLinearColor(R, G, B, 1.0f);
            }
        }
        
        // Parse sun intensity
        if (LightObj->HasField(TEXT("SunIntensity")))
        {
            Environment.Lighting.SunIntensity = LightObj->GetNumberField(TEXT("SunIntensity"));
        }
        
        // Parse sun angle
        if (LightObj->HasField(TEXT("SunPitch")))
        {
           Environment.Lighting.SunPitch = LightObj->GetNumberField(TEXT("SunPitch"));
        }
        
        if (LightObj->HasField(TEXT("SunYaw")))
        {
            Environment.Lighting.SunYaw = LightObj->GetNumberField(TEXT("SunYaw"));
        }
        
        // Parse sky light
        if (LightObj->HasField(TEXT("SkyLightColor")))
        {
            const TArray<TSharedPtr<FJsonValue>>* SkyColorArray;
            if (LightObj->TryGetArrayField(TEXT("SkyLightColor"), SkyColorArray) && SkyColorArray->Num() >= 3)
            {
                float R = (*SkyColorArray)[0]->AsNumber() / 255.0f;
                float G = (*SkyColorArray)[1]->AsNumber() / 255.0f;
                float B = (*SkyColorArray)[2]->AsNumber() / 255.0f;
                Environment.Lighting.SkyLightColor = FLinearColor(R, G, B, 1.0f);
            }
        }
        
        if (LightObj->HasField(TEXT("SkyLightIntensity")))
        {
            Environment.Lighting.SkyLightIntensity = LightObj->GetNumberField(TEXT("SkyLightIntensity"));
        }
        
        // Parse temperature
        if (LightObj->HasField(TEXT("SunTemperature")))
        {
            Environment.Lighting.SunTemperature = LightObj->GetNumberField(TEXT("SunTemperature"));
            Environment.Lighting.bUseTemperature = true;
        }
        
        UE_LOG(LogTemp, Display, TEXT("JsonParser: Parsed lighting settings"));
    }
}

void UJsonParser::ParsePropModification(const TSharedPtr<FJsonObject>& JsonObject, FPropsModification& PropMod)
{
    if (!JsonObject.IsValid())
    {
        return;
    }

    // Parse TagName
    if (JsonObject->HasField(TEXT("TagName")))
    {
        PropMod.TagName = JsonObject->GetStringField(TEXT("TagName"));
    }

    // Parse PropColor
    if (JsonObject->HasField(TEXT("PropColor")))
    {
        const TArray<TSharedPtr<FJsonValue>>* ColorArray;
        if (JsonObject->TryGetArrayField(TEXT("PropColor"), ColorArray) && ColorArray->Num() == 3)
        {
            int32 R = (*ColorArray)[0]->AsNumber();
            int32 G = (*ColorArray)[1]->AsNumber();
            int32 B = (*ColorArray)[2]->AsNumber();
            PropMod.PropColor = FColor(R, G, B);
        }
    }

    // Parse Texture object
    if (JsonObject->HasField(TEXT("Texture")))
    {
        const TSharedPtr<FJsonObject>* TextureObject;
        if (JsonObject->TryGetObjectField(TEXT("Texture"), TextureObject))
        {
            ParseTextureSet(*TextureObject, PropMod.Texture);
        }
    }

    // Parse ParticleEffects
    if (JsonObject->HasField(TEXT("ParticleEffects")))
    {
        PropMod.ParticleEffects = JsonObject->GetStringField(TEXT("ParticleEffects"));
    }

    UE_LOG(LogTemp, Display, TEXT("PARSER: PropMod - Tag: %s, ParticleEffects: %s"), 
           *PropMod.TagName, *PropMod.ParticleEffects);
}

void UJsonParser::ParseTextureSet(const TSharedPtr<FJsonObject>& JsonObject, FTextureSet& TextureSet)
{
    if (!JsonObject.IsValid())
    {
        return;
    }

    // Parse all texture paths
    if (JsonObject->HasField(TEXT("BaseColorPath")))
    {
        TextureSet.BaseColorPath = JsonObject->GetStringField(TEXT("BaseColorPath"));
    }

    if (JsonObject->HasField(TEXT("NormalPath")))
    {
        TextureSet.NormalPath = JsonObject->GetStringField(TEXT("NormalPath"));
    }

    if (JsonObject->HasField(TEXT("RoughnessPath")))
    {
        TextureSet.RoughnessPath = JsonObject->GetStringField(TEXT("RoughnessPath"));
    }

    if (JsonObject->HasField(TEXT("MetallicPath")))
    {
        TextureSet.MetallicPath = JsonObject->GetStringField(TEXT("MetallicPath"));
    }

    if (JsonObject->HasField(TEXT("AOPath")))
    {
        TextureSet.AOPath = JsonObject->GetStringField(TEXT("AOPath"));
    }

    if (JsonObject->HasField(TEXT("DisplacementPath")))
    {
        TextureSet.DisplacementPath = JsonObject->GetStringField(TEXT("DisplacementPath"));
    }

    if (JsonObject->HasField(TEXT("OpacityPath")))
    {
        TextureSet.OpacityPath = JsonObject->GetStringField(TEXT("OpacityPath"));
    }

    UE_LOG(LogTemp, Display, TEXT("PARSER: TextureSet - BaseColor: %s, Normal: %s"), 
           *TextureSet.BaseColorPath, *TextureSet.NormalPath);
}

void UJsonParser::ParseSpawnRequest(const TSharedPtr<FJsonObject>& JsonObject, FSpawnRequest& SpawnRequest)
{
   
    if (!JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("JsonParser::ParseSpawnRequest: Invalid JSON object"));
        return;
    }

    
    if (JsonObject->HasField(TEXT("AssetPath")))
    {
        SpawnRequest.AssetPath = JsonObject->GetStringField(TEXT("AssetPath"));
        UE_LOG(LogTemp, Display, TEXT("JsonParser: AssetPath = '%s'"), *SpawnRequest.AssetPath);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("JsonParser: SpawnRequest missing REQUIRED 'AssetPath' field"));
    }
    
  
    if (JsonObject->HasField(TEXT("ObjectName")))
    {
        SpawnRequest.ObjectName = JsonObject->GetStringField(TEXT("ObjectName"));
        UE_LOG(LogTemp, Display, TEXT("JsonParser: ObjectName = '%s'"), *SpawnRequest.ObjectName);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("JsonParser: SpawnRequest missing REQUIRED 'ObjectName' field"));
    }
    
   
    if (JsonObject->HasField(TEXT("LocationName")))
    {
        SpawnRequest.LocationName = JsonObject->GetStringField(TEXT("LocationName"));
        UE_LOG(LogTemp, Display, TEXT("JsonParser: LocationName = '%s' (will be resolved by SceneStateTracker)"), 
            *SpawnRequest.LocationName);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("JsonParser: SpawnRequest missing REQUIRED 'LocationName' field"));
    }
    
    
    if (JsonObject->HasField(TEXT("LocationOffset")))
    {
        const TArray<TSharedPtr<FJsonValue>>* OffsetArray;
        if (JsonObject->TryGetArrayField(TEXT("LocationOffset"), OffsetArray) && OffsetArray->Num() == 3)
        {
            SpawnRequest.LocationOffset.X = (*OffsetArray)[0]->AsNumber();
            SpawnRequest.LocationOffset.Y = (*OffsetArray)[1]->AsNumber();
            SpawnRequest.LocationOffset.Z = (*OffsetArray)[2]->AsNumber();
            
            UE_LOG(LogTemp, Display, TEXT("JsonParser: LocationOffset = [%.2f, %.2f, %.2f]"),
                SpawnRequest.LocationOffset.X, 
                SpawnRequest.LocationOffset.Y, 
                SpawnRequest.LocationOffset.Z);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("JsonParser: LocationOffset array invalid - expected [X, Y, Z]"));
            // Keep default FVector::ZeroVector (no offset)
        }
    }
   
    if (JsonObject->HasField(TEXT("Rotation")))
    {
        const TArray<TSharedPtr<FJsonValue>>* RotationArray;
        if (JsonObject->TryGetArrayField(TEXT("Rotation"), RotationArray) && RotationArray->Num() == 3)
        {
            SpawnRequest.Rotation.Pitch = (*RotationArray)[0]->AsNumber();
            SpawnRequest.Rotation.Yaw   = (*RotationArray)[1]->AsNumber();
            SpawnRequest.Rotation.Roll  = (*RotationArray)[2]->AsNumber();
            
            UE_LOG(LogTemp, Display, TEXT("JsonParser: Rotation = [%.2f, %.2f, %.2f] (P, Y, R)"),
                SpawnRequest.Rotation.Pitch,
                SpawnRequest.Rotation.Yaw,
                SpawnRequest.Rotation.Roll);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("JsonParser: Rotation array invalid - expected [Pitch, Yaw, Roll]"));
            // Keep default FRotator::ZeroRotator
        }
    }
    
    if (JsonObject->HasField(TEXT("Scale")))
    {
        const TArray<TSharedPtr<FJsonValue>>* ScaleArray;
        if (JsonObject->TryGetArrayField(TEXT("Scale"), ScaleArray) && ScaleArray->Num() == 3)
        {
            SpawnRequest.Scale.X = (*ScaleArray)[0]->AsNumber();
            SpawnRequest.Scale.Y = (*ScaleArray)[1]->AsNumber();
            SpawnRequest.Scale.Z = (*ScaleArray)[2]->AsNumber();
            
            UE_LOG(LogTemp, Display, TEXT("JsonParser: Scale = [%.2f, %.2f, %.2f]"),
                SpawnRequest.Scale.X,
                SpawnRequest.Scale.Y,
                SpawnRequest.Scale.Z);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("JsonParser: Scale array invalid - expected [X, Y, Z]"));
            // Default to 1.0
            SpawnRequest.Scale = FVector::OneVector;
        }
    }
    else
    {
        // If not specified, default to original size (1.0, 1.0, 1.0)
        SpawnRequest.Scale = FVector::OneVector;
    }
    
   
    if (JsonObject->HasField(TEXT("Tag")))
    {
        SpawnRequest.Tag = JsonObject->GetStringField(TEXT("Tag"));
        UE_LOG(LogTemp, Display, TEXT("JsonParser: Tag = '%s'"), *SpawnRequest.Tag);
    }

    if (JsonObject->HasField(TEXT("ClearanceRadius")))
    {
        SpawnRequest.ClearanceRadius = JsonObject->GetNumberField(TEXT("ClearanceRadius"));
        
        if (SpawnRequest.ClearanceRadius <= 0.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("JsonParser: ClearanceRadius %.2f is invalid (must be > 0), using default"),
                SpawnRequest.ClearanceRadius);
            SpawnRequest.ClearanceRadius = 150.0f; // Use default
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("JsonParser: ClearanceRadius = %.2f"), SpawnRequest.ClearanceRadius);
        }
    }
    else
    {
        // If not specified, use default 150 units (defined in FSpawnRequest struct)
        // This is a reasonable default for most objects
        UE_LOG(LogTemp, Display, TEXT("JsonParser: ClearanceRadius not specified, using default 150.0"));
    }
    
   
    // Log complete spawn request summary
    UE_LOG(LogTemp, Display, TEXT("JsonParser: ✅ Parsed SpawnRequest complete"));
    UE_LOG(LogTemp, Display, TEXT("   Asset='%s', Object='%s', Location='%s' (semantic)"),
        *SpawnRequest.AssetPath, 
        *SpawnRequest.ObjectName,
        *SpawnRequest.LocationName);
    UE_LOG(LogTemp, Display, TEXT("   Rotation=[%.0f°, %.0f°, %.0f°], Scale=[%.1f, %.1f, %.1f]"),
        SpawnRequest.Rotation.Pitch,
        SpawnRequest.Rotation.Yaw,
        SpawnRequest.Rotation.Roll,
        SpawnRequest.Scale.X,
        SpawnRequest.Scale.Y,
        SpawnRequest.Scale.Z);
}


