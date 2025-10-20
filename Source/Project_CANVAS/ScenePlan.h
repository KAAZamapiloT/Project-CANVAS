// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Common/TypedElementCommonTypes.h"
#include "GameFramework/Actor.h"
#include"ScenePlan.generated.h"
/**
 *
 *FEnhancedScenePlan -> IT IS A DATA STRUCTURE THAT DECIDES HOW OUR SCENE WILL GET BUILF THINK OF IT AS INGERIDENTS AND RECIPE
 *
 * THINK OF IT AS A OUTPUT OF A MACHINE WHICH OUR JSON PARSER WILL GIVE
 */

/*
 * OUR SCENE BUILDER WILL FOLLOW THIS TO MAKE MODIFICATION IN OUR SCENE
 */

/**
* A PBR texture set. All paths are FStrings from the Asset Indexer.
* The SceneBuilder will asynchronously load these.
*/

USTRUCT(BlueprintType)
struct FTextureSet
{
	GENERATED_BODY()
	// Core maps
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString BaseColorPath;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString NormalPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString RoughnessPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString MetallicPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString AOPath;

	// Optional extras
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplacementPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString OpacityPath;
};


/* Defines global environmental settings.
 */
USTRUCT(BlueprintType)
struct FEnvironmentPlan
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float FogDensity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor FogColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString PostProcessingName;
};

// Defines all modifications for a single prop or group of props.
USTRUCT(BlueprintType)
struct FPropsModification
{
	GENERATED_BODY()
	public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString TagName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor PropColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTextureSet Texture;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ParticleEffects;
};


USTRUCT(BlueprintType)
struct FEnhancedScenePlan
{
	GENERATED_BODY()
	public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ThemeName;

	// All environmental changes
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	struct FEnvironmentPlan Environment;
	
	// A list of all props to change
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FPropsModification> Props;
	
	
};

