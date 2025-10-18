// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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
USTRUCT(BlueprintType)
struct FEnhancedScenePlan
{
	GENERATED_BODY()
	public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString ThemeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor BackgroundColor;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor TextColor;
	
};

