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
    // a pointer that holds our parsed data
	TSharedPtr<FJsonObject> JsonObject;

	// json rader to read a raw string

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContext);

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		if (JsonObject->HasField(TEXT("BackgroundColor")))
		{
			const TArray<TSharedPtr<FJsonValue>>* ColorArray;

			// Try to read the field as an array; also make sure it has 3 elements
			if (JsonObject->TryGetArrayField(TEXT("BackgroundColor"), ColorArray) && ColorArray->Num() == 3)
			{
				// Convert each value in the array to an integer (R, G, B)
				int32 R = (*ColorArray)[0]->AsNumber();
				int32 G = (*ColorArray)[1]->AsNumber();
				int32 B = (*ColorArray)[2]->AsNumber();

				// Assign to our struct as an FColor
				Plan.BackgroundColor = FColor(R, G, B);
			}
		}

		if (JsonObject->HasField(TEXT("TextColor")))
		{
			const TArray<TSharedPtr<FJsonValue>>* ColorArray;

			if (JsonObject->TryGetArrayField(TEXT("TextColor"), ColorArray) && ColorArray->Num() == 3)
			{
				int32 R = (*ColorArray)[0]->AsNumber();
				int32 G = (*ColorArray)[1]->AsNumber();
				int32 B = (*ColorArray)[2]->AsNumber();
				Plan.TextColor = FColor(R, G, B);
			}
			
			
		}

		if (JsonObject->HasField(TEXT("ThemeName")))
		{
			Plan.ThemeName = JsonObject->GetStringField(TEXT("ThemeName"));
		}

		if (JsonObject->HasField(TEXT("TextureName")))
		{
			Plan.TextureName = JsonObject->GetStringField(TEXT("TextureName"));
		}
		UE_LOG(LogTemp, Warning, TEXT("PARSER: Extracted TextureName: %s"), *Plan.TextureName);
		FString LogName=Plan.ThemeName;
		UE_LOG(LogTemp, Display, TEXT("%s"), *LogName);
		return Plan;
	}else
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to parse JsonContext in CreatePlan()"));
		Plan.BackgroundColor = FColor::Red;
		Plan.TextColor = FColor::Red;
		Plan.ThemeName = FString("Background");
		return Plan;
	}

	
	return Plan;
	
}
