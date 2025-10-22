// CombatAnimationComponent.h
// Handles execution of Action Commands: plays montages, tracks state, fires delegates
// This is the "executor" layer - it consumes Decision Engine output and drives animation

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatData.h"
#include "CombatAnimationComponent.generated.h"

/**
 * Delegate fired when a montage/move completes
 * Broadcasts the completed move identifier for combo chaining logic
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatMontageEnded, FName, CompletedMove);

/**
 * Delegate fired when AnimNotify_CombatHit is triggered during montage
 * Broadcasts cached damage/stun values for hit detection and application
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitWindowActive, float, Damage, float, Stun);

/**
 * UCombatAnimationComponent
 * 
 * Core animation execution system for the combo-driven combat design.
 * Responsibilities:
 * - Play montages from FActionCommand output
 * - Track current move execution state
 * - Cache damage/stun for AnimNotify callbacks
 * - Broadcast completion for combo chaining
 * 
 * Usage:
 * - Attach to player and enemy characters
 * - Call ExecuteActionCommand() with Decision Engine output
 * - Bind OnMontageEnded for combo chain continuation
 * - Bind OnHitWindowActive for damage application
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PROJECT_CANVAS_API UCombatAnimationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatAnimationComponent();

    /**
     * Execute an Action Command from the Decision Engine
     * Plays the specified montage and caches damage/stun for notify callbacks
     * 
     * @param Command - The action to execute (contains montage, damage, etc.)
     */
    UFUNCTION(BlueprintCallable, Category="Combat Animation")
    void ExecuteActionCommand(const FActionCommand& Command);

    /**
     * Stop the currently playing montage and clear state
     * Used for interruptions (e.g., getting hit, blocking)
     */
    UFUNCTION(BlueprintCallable, Category="Combat Animation")
    void StopCurrentAction();

    /**
     * Check if a move is currently being executed
     * Used to prevent input during active animations
     * 
     * @return True if a move is active, false otherwise
     */
    UFUNCTION(BlueprintPure, Category="Combat Animation")
    bool IsExecutingMove() const { return !CurrentMoveIdentifier.IsNone(); }

    /**
     * Get elapsed time in the current move (in seconds)
     * Used for cancel window checks and input buffering
     * 
     * @return Time since move started, or 0 if no move active
     */
    UFUNCTION(BlueprintPure, Category="Combat Animation")
    float GetMoveElapsedTime() const;

    /** Fired when a montage/move completes - used for combo chaining */
    UPROPERTY(BlueprintAssignable, Category="Combat Animation")
    FOnCombatMontageEnded OnMontageEnded;

    /** Fired when AnimNotify_CombatHit triggers - used for damage application */
    UPROPERTY(BlueprintAssignable, Category="Combat Animation")
    FOnHitWindowActive OnHitWindowActive;

    /** Identifier of the currently executing move (NAME_None if idle) */
    UPROPERTY(BlueprintReadOnly, Category="Combat Animation")
    FName CurrentMoveIdentifier;

protected:
    /** Cached reference to owning character */
    UPROPERTY()
    class ACharacter* OwnerCharacter;

    /** Cached reference to character's anim instance */
    UPROPERTY()
    class UAnimInstance* AnimInstance;

    /** Cached damage value from current Action Command */
    float CachedDamage;

    /** Cached stun duration from current Action Command */
    float CachedStunDuration;

    /** Timestamp when current move started (for elapsed time calculation) */
    float MoveStartTime;

    virtual void BeginPlay() override;

    /**
     * Callback bound to montage end delegate
     * Clears state and broadcasts OnMontageEnded
     * 
     * @param Montage - The montage that completed
     * @param bInterrupted - True if montage was stopped early
     */
    UFUNCTION()
    void OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted);

public:
    /**
     * Called by AnimNotify_CombatHit during montage playback
     * Broadcasts OnHitWindowActive with cached damage/stun values
     */
    UFUNCTION()
    void HandleHitNotify();
};