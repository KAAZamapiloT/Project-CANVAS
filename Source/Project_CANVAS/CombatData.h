// Fill out your copyright notice in the Description page of Project Settings.

// CombatData.h
// Defines all data structures for the combat system: Context Vector, Action Command, and Move Data
// These structs implement the data-driven architecture from your design document

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "CombatData.generated.h"

/**
 * Input direction for combo chains
 * Used in FollowUpMoves map to determine next move based on directional input
 */
UENUM(BlueprintType)
enum class EInputDirection: uint8
{
    EID_UP UMETA(DisplayName = "Up"),
    EID_DOWN UMETA(DisplayName = "Down"),
    EID_LEFT UMETA(DisplayName = "Left"),
    EID_RIGHT UMETA(DisplayName = "Right"),
    EID_NEUTRAL UMETA(DisplayName = "Neutral")  // No direction, just button press
};

/**
 * Combat range categories for decision-making
 * Used by Decision Engine to select range-appropriate moves
 */
UENUM(BlueprintType)
enum class ECombatRange: uint8
{
    ECR_FAR UMETA(DisplayName = "Far"),    // > 400 units
    ECR_MID UMETA(DisplayName = "Mid"),    // 150-400 units
    ECR_NEAR UMETA(DisplayName = "Near")   // < 150 units
};

/**
 * FMoveData - Row structure for DT_CombatMoves DataTable
 * Each row represents a single combat move with all its properties
 * This is the core of the data-driven combat design
 */
USTRUCT(BlueprintType)
struct FMoveData : public FTableRowBase
{
    GENERATED_BODY()

    /** Animation montage to play when this move executes */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    UAnimMontage* AnimationToPlay = nullptr;

    /** Base damage this move deals on hit */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    float Damage = 10.f;

    /** Duration the target is stunned after being hit (in seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    float HitStunDuration = 0.3f;

    /** Cooldown before this move can be used again (in seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    float Cooldown = 0.5f;

    /** Gameplay tags the enemy must have for this move to be selected by AI */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context Rules")
    FGameplayTagContainer RequiredEnemyStateTags;

    /** Optimal range for this move - Decision Engine prefers moves matching current distance */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Context Rules")
    ECombatRange OptimalRange = ECombatRange::ECR_MID;

    /** 
     * Map of follow-up moves for combo chaining
     * Key: Input direction during this move
     * Value: Name of the next move to chain into
     * Example: {EID_NEUTRAL -> "HeavyAttack"} means neutral input chains to HeavyAttack
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo System")
    TMap<EInputDirection, FName> FollowUpMoves;

    /** Priority when multiple moves are valid (higher = preferred) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    int32 Priority = 0;

    /** 
     * Window during move execution when cancel/follow-up is allowed (in seconds)
     * Input buffering works within this window
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move Data")
    float ExecutionTime = 0.5f;
};

/**
 * FContextVector - Input to the Combat Decision Engine
 * Aggregates all state needed to make intelligent move selection
 * Assembled fresh before each decision query
 */
USTRUCT(BlueprintType)
struct FContextVector
{
    GENERATED_BODY()

    /** Current player input (e.g., "LightAttack", "HeavyAttack") */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FName CurrentInput = NAME_None;

    /** The move that just completed (used for combo chaining via FollowUpMoves) */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FName LastMoveExecuted = NAME_None;

    /** Current state tags for the player (e.g., "State.Airborne", "State.Blocking") */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FGameplayTagContainer PlayerStateTags;

    /** Current state tags for the enemy (e.g., "State.Stunned", "State.Blocking") */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FGameplayTagContainer EnemyStateTags;

    /** Distance to enemy in Unreal units */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    float DistanceToEnemy = 0.f;

    /** Moves currently on cooldown (by FName identifier) */
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    TArray<FName> ActiveCooldowns;
};

/**
 * FActionCommand - Output from the Combat Decision Engine
 * Contains everything needed to execute a single combat action
 * Consumed by CombatAnimationComponent to drive montage playback and damage
 */
USTRUCT(BlueprintType)
struct FActionCommand
{
    GENERATED_BODY()

    /** Identifier of the move being executed (row name from DataTable) */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    FName MoveIdentifier = NAME_None;

    /** Animation montage to play */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    UAnimMontage* AnimationToPlay = nullptr;

    /** Damage to apply when hit notify fires */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    float DamageToApply = 0.f;

    /** Stun duration to apply to target on hit */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    float StunDurationToInflict = 0.f;

    /** Movement impulse to apply (e.g., for dash attacks) */
    UPROPERTY(BlueprintReadWrite, Category = "Action")
    FVector MovementToApply = FVector::ZeroVector;
};

