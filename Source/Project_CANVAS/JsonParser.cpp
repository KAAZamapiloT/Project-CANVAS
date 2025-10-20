// Fill out your copyright notice in the Description page of Project Settings.


#include "JsonParser.h"

#include <rapidjson/reader.h>

#include "AudioMixerBlueprintLibrary.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "GeometryCollection/GeometryCollectionParticlesData.h"


bool UJsonParser::bScehmaValidation(FString& JsonContext)
{
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