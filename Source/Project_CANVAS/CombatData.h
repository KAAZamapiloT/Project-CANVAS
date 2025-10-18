// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include"GameplayTagContainer.h"
#include "CombatData.generated.h"

UENUM(BlueprintType)

enum class EInputDirection: uint8
{
	EID_UP,EID_DOWN,EID_LEFT,EID_RIGHT
};


UENUM(BlueprintType)

enum class ECombatRange:uint8
{
	ECR_FAR,ECR_NEAR,ECR_MID
};

USTRUCT(BlueprintType)
struct FMoveData : public FTableRowBase
{
    GENERATED_BODY()

public:
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    UAnimMontage* AnimationToPlay;

    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    float Damage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    float HitStunDuration;
    

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    float Cooldown;

   
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context Rules")
    FGameplayTagContainer RequiredEnemyStateTags;

 
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo Chaining")
    TMap<EInputDirection, FName> FollowUpMoves;
};



USTRUCT(BlueprintType)
struct FActionCommand
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Action Command")
    UAnimMontage* AnimationToPlay = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Action Command")
    float DamageToApply = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Action Command")
    float StunDurationToInflict = 0.0f;
    
    UPROPERTY(BlueprintReadOnly, Category = "Action Command")
    FName MoveIdentifier; 
};

