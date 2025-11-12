// CombatStateComponent.h
// Centralized Context Vector builder for combat system
// Queries player/enemy state, distance, tags, cooldowns
// Single responsibility: assemble FContextVector from current game state

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatData.h"
#include "CombatStateComponent.generated.h"

// Forward declarations
class UCombatAnimationComponent;
class ACharacter;
class UCharacterMovementComponent;

/**
 * UCombatStateComponent
 * 
 * Tracks combat state and builds Context Vector for Decision Engine queries.
 * This is the "context updater" from your design document.
 * 
 * Responsibilities:
 * - Query player and enemy positions for distance calculation
 * - Track last move executed from CombatAnimationComponent
 * - Gather gameplay tags from both characters
 * - Track active cooldowns
 * - Build complete FContextVector on demand
 * 
 * Usage:
 * - Attach to player character
 * - Call BuildContext() before querying Decision Engine
 * - Automatically queries cached references for current state
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PROJECT_CANVAS_API UCombatStateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatStateComponent();

    /**
     * Build complete Context Vector from current game state
     * Queries enemy location, player state tags, last move, etc.
     * Call this before every Decision Engine query
     * 
     * @param CurrentInput - The input action triggering this context (e.g., "LightAttack")
     * @return Fully populated FContextVector ready for Decision Engine
     */
    UFUNCTION(BlueprintCallable, Category="Combat State")
    FContextVector BuildContext(FName CurrentInput);

    /**
     * Set the enemy reference for distance and state queries
     * Called from character BeginPlay or when enemy spawns
     * 
     * @param Enemy - The enemy character to track
     */
    UFUNCTION(BlueprintCallable, Category="Combat State")
    void SetEnemy(ACharacter* Enemy);

    /**
     * Add a move to cooldown tracking
     * Called when a move executes
     * 
     * @param MoveName - Identifier of the move on cooldown
     * @param Duration - How long the cooldown lasts (seconds)
     */
    UFUNCTION(BlueprintCallable, Category="Combat State")
    void StartCooldown(FName MoveName, float Duration);

    /**
     * Check if a move is currently on cooldown
     * 
     * @param MoveName - Move identifier to check
     * @return True if move is still on cooldown
     */
    UFUNCTION(BlueprintPure, Category="Combat State")
    bool IsOnCooldown(FName MoveName) const;

    UFUNCTION(BlueprintCallable, Category="Combat State")
   EInputDirection CalculateEnemyDirection();

    UFUNCTION(BlueprintCallable, Category="Combat State")
    ACharacter* GetEnemy(){return EnemyCharacter;}
    
protected:
    /** Cached reference to owner character */
    UPROPERTY()
    ACharacter* OwnerCharacter;

    /** Cached reference to owner's CombatAnimationComponent */
    UPROPERTY()
    UCombatAnimationComponent* OwnerCombatAnimComp;

    /** Cached reference to enemy character */
    UPROPERTY()
    ACharacter* EnemyCharacter;

    /** Map of moves on cooldown with expiration time */
    UPROPERTY()
    TMap<FName, float> ActiveCooldowns;

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
public:
    /**
     * Calculate distance between owner and enemy
     * 
     * @return Distance in Unreal units, or 0 if no enemy cached
     */
    float GetDistanceToEnemy() const;

    /**
     * Get gameplay tags from a character's components
     * 
     * @param Character - Character to query tags from
     * @return Container of gameplay tags (State.Stunned, State.Airborne, etc.)
     */
    FGameplayTagContainer GetCharacterTags(ACharacter* Character) const;
};
