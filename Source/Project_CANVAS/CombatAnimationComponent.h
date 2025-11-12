// CombatAnimationComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatData.h"
#include "CombatAnimationComponent.generated.h"

// Delegate fired when a montage/move completes (for combo chaining)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatMontageEnded, FName, CompletedMove);

// Delegate fired when AnimNotify_CombatHit is triggered during montage
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitWindowActive, float, Damage, float, Stun);

UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PROJECT_CANVAS_API UCombatAnimationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatAnimationComponent();

    /** Execute an Action Command from the Decision Engine
     * Plays montage and caches damage/stun for notifies
     */
    UFUNCTION(BlueprintCallable, Category="Combat Animation")
    void ExecuteActionCommand(const FActionCommand& Command);

    /** Stop the currently playing montage and clear state (interruptions, blocking) */
    UFUNCTION(BlueprintCallable, Category="Combat Animation")
    void StopCurrentAction();

    /** Is a move currently being executed (prevents double input) */
    UFUNCTION(BlueprintPure, Category="Combat Animation")
    bool IsExecutingMove() const { return !CurrentMoveIdentifier.IsNone(); }

    /** Is any montage currently playing (for AI/player) */
    UFUNCTION(BlueprintPure, Category="Combat Animation")
    bool IsPlayingMontage() const;

    /** Is the animation component in valid state for execution */
    UFUNCTION(BlueprintPure, Category="Combat Animation")
    bool IsValidForExecution() const;

    /** Time elapsed in the current move (seconds) */
    UFUNCTION(BlueprintPure, Category="Combat Animation")
    float GetMoveElapsedTime() const;

    /** Fired when a montage/move completes (used for combo chaining) */
    UPROPERTY(BlueprintAssignable, Category="Combat Animation")
    FOnCombatMontageEnded OnMontageEnded;

    /** Fired when AnimNotify_CombatHit triggers (used for damage application) */
    UPROPERTY(BlueprintAssignable, Category="Combat Animation")
    FOnHitWindowActive OnHitWindowActive;

    /** Identifier of the currently executing move (NAME_None if idle) */
    UPROPERTY(BlueprintReadOnly, Category="Combat Animation")
    FName CurrentMoveIdentifier;

protected:
    /** Cached reference to owning character */
    UPROPERTY()
    class ACharacter* OwnerCharacter;

    /** Cached reference to anim instance */
    UPROPERTY()
    class UAnimInstance* AnimInstance;

    /** Cached damage value from current Action Command */
    float CachedDamage;

    /** Cached stun duration from current Action Command */
    float CachedStunDuration;

    /** Timestamp when current move started */
    float MoveStartTime;

    virtual void BeginPlay() override;

    /** Callback bound to montage end delegate; state cleanup and broadcast */
    UFUNCTION()
    void OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted);

public:
    /** Called by AnimNotify_CombatHit during montage playback */
    UFUNCTION()
    void HandleHitNotify();

private:
    /** Helper function to determine if this is Player or Enemy (for logging for now) */
    FString GetOwnerType() const;

};
