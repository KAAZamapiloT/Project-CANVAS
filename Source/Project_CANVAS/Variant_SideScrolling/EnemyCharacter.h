// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatData.h"
#include "Damagable.h"
#include "EnemyCharacter.generated.h"

// Forward declarations
class UHealthComponent;
class UCombatAnimationComponent;
class UCombatStateComponent;
class UCombatDecisionEngine;
class UBehaviorTree;
class AAIController;
class UAnimMontage;  // ✅ ADDED: Required for OnMontageCompleted delegate signature

/**
 * @enum EEnemyState
 * @brief Enemy behavioral state for AI control
 */
UENUM(BlueprintType)
enum class EEnemyState : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Patrolling UMETA(DisplayName = "Patrolling"),
    Attacking UMETA(DisplayName = "Attacking"),
    Stunned UMETA(DisplayName = "Stunned"),
    Dead UMETA(DisplayName = "Dead")
};

/**
 * @class AEnemyCharacter
 * @brief AI-controlled sidescroller combat character with Behavior Tree support
 * 
 * @details
 * **Architecture:**
 * - Component-based design with zero redundant logic
 * - All state queries delegated to specialized components
 * - Hybrid AI: Behavior Tree (preferred) or Timer (fallback)
 * 
 * **Component Delegation:**
 * - Health/Stun/Alive → UHealthComponent
 * - Animation/Execution → UCombatAnimationComponent  
 * - Context Building → UCombatStateComponent
 * - Decision Making → UCombatDecisionEngine
 * 
 * **AI Modes:**
 * 1. BehaviorTree assigned → BT controls combat via tasks
 * 2. No BehaviorTree → Timer-based fallback mode
 * 
 * @see UHealthComponent
 * @see UCombatAnimationComponent
 * @see UCombatStateComponent
 * @see UCombatDecisionEngine
 */
UCLASS()
class PROJECT_CANVAS_API AEnemyCharacter : public ACharacter, public IDamagable
{
    GENERATED_BODY()

public:
    /** @brief Constructor - configures sidescroller movement and AI possession */
    AEnemyCharacter();

    /** @brief Initialize combat components and start AI behavior */
    virtual void BeginPlay() override;

    /** @brief Validate player reference and alive state */
    virtual void Tick(float DeltaTime) override;

    /** @brief Cleanup timers on end play */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    
    
    
    // ========================================
    // COMBAT CONFIGURATION
    // ========================================

    /**
     * @brief Maximum health points (applied to HealthComponent on spawn)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Config")
    float MaxHealth = 100.0f;

    /**
     * @brief Time between AI decisions in timer mode (seconds)
     * @note Ignored when BehaviorTree is assigned
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Config")
    float DecisionInterval = 1.5f;

    /**
     * @brief Minimum attack range for CanAttackPlayer() check (cm)
     * @note Sidescroller: X-axis distance only
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Config")
    float MinAttackDistance = 200.0f;

    /**
     * @brief DataTable containing enemy-specific moves (FActionCommand rows)
     * @details Passed to CombatDecisionEngine for move selection
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Config")
    class UDataTable* EnemyMoveDataTable = nullptr;

    // ========================================
    // COMBAT COMPONENTS
    // ========================================

    /**
     * @brief Health, armor, stun, and regeneration system
     * @note Use HealthComponent->IsAlive(), IsStunned(), GetHealthPercentage()
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UHealthComponent* HealthComponent = nullptr;

    /**
     * @brief Animation execution component
     * @note Use CombatAnimationComponent->IsExecutingMove() to check busy state
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UCombatAnimationComponent* CombatAnimationComponent = nullptr;

    /**
     * @brief Context builder for decision engine
     * @note Use CombatStateComponent->BuildContext() before decisions
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UCombatStateComponent* CombatStateComponent = nullptr;

    /**
     * @brief AI decision engine (analyzes context, selects moves)
     * @note Use CombatDecisionEngine->DecideNextMove() with context
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UCombatDecisionEngine* CombatDecisionEngine = nullptr;

    // ========================================
    // COMBAT CONTROL
    // ========================================

    /**
     * @brief Start timer-based combat loop (fallback mode)
     * @details Only activates if BehaviorTree is not assigned
     */
    UFUNCTION(BlueprintCallable, Category="Combat")
    void StartCombatBehavior();

    /**
     * @brief Stop combat timer loop
     */
    UFUNCTION(BlueprintCallable, Category="Combat")
    void StopCombatBehavior();

    /**
     * @brief Execute AI decision cycle
     * @details 
     * 1. CombatStateComponent builds context
     * 2. CombatDecisionEngine selects move
     * 3. CombatAnimationComponent executes move
     * 
     * Called by: Timer (fallback) or BT Task (preferred)
     */
    UFUNCTION(BlueprintCallable, Category="Combat")
    void MakeCombatDecision();

    // ========================================
    // STATE QUERIES (Delegated to Components)
    // ========================================

    /**
     * @brief Get current enemy behavioral state
     * @return State based on component queries (delegated logic)
     */
    UFUNCTION(BlueprintPure, Category="Combat")
    EEnemyState GetEnemyState() const;

    /**
     * @brief Check if can attack player now
     * @return True if in range and not busy
     * @note Uses CombatStateComponent->GetDistanceToEnemy() and CombatAnimationComponent->IsExecutingMove()
     */
    UFUNCTION(BlueprintPure, Category="Combat")
    bool CanAttackPlayer() const;

    // ========================================
    // AI HELPERS
    // ========================================

    /**
     * @brief Check if Behavior Tree is controlling this enemy
     * @return True if BT brain component is active
     */
    UFUNCTION(BlueprintPure, Category="AI")
    bool IsBehaviorTreeActive() const;

    /**
     * @brief Get AI controller possessing this pawn
     * @return AIController pointer or nullptr
     */
    UFUNCTION(BlueprintPure, Category="AI")
    AAIController* GetAIController() const;

    // ========================================
    // IDAMAGABLE INTERFACE
    // ========================================

    /**
     * @brief Receive damage (delegated to HealthComponent)
     * @param Spec Damage specification
     */
    virtual void ReceiveDamage_Implementation(const FDamageSpec& Spec) override;

    /**
     * @brief Check if alive (delegated to HealthComponent)
     * @return True if health > 0
     */
    virtual bool IsAlive_Implementation() const override;
    /**
     * @brief Execute a combat move by name (AI/BT callable)
     * @param MoveName Name of move to execute from DataTable
     * 
     * @details
     * Mirrors player's ExecuteMove() but uses CombatDecisionEngine
     * to build context and execute via CombatAnimationComponent
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ExecuteMove(FName MoveName);

    /**
 * @brief Called when montage ends (matches OnMontageEnded signature)
 * @param MontageName Name of the montage that ended
 */
    UFUNCTION()
    void OnMontageEndedHandler(FName MontageName);

    /**
 * @brief Called when enemy takes damage
 * @param DamageAmount Amount of damage received
 * @param HitLocation Source of damage
 */
    UFUNCTION()
    void OnDamageTakenHandler(float DamageAmount, FVector HitLocation);

    /** * Resets the enemy from Ragdoll state back to Fighting state.
         * Re-attaches mesh, resets health, and restarts AI.
         */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResetEnemyState();
protected:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** @brief Player reference for combat targeting */
    UPROPERTY()
    class ACharacter* PlayerCharacter = nullptr;

    /** @brief Timer handle for fallback mode decision loop */
    FTimerHandle DecisionTimerHandle;

    /** @brief Combat behavior active flag */
    bool bIsInCombat = false;

    // ========================================
    // INITIALIZATION
    // ========================================

    /** @brief Create and register all combat components */
    void InitializeCombatComponents();

    /** @brief Bind component event delegates */
    void BindCombatDelegates();

    /** 
     * @brief Callback when animation montage completes
     * @param Montage The montage that finished
     * @param bInterrupted Whether montage was interrupted
     */
    UFUNCTION()
    void OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted);

    // ✅ ADDED: Handler for OnHealthDepleted delegate
    /**
     * @brief Callback when health reaches zero
     * @details Stops combat behavior and triggers death logic
     */
    UFUNCTION()
    void OnHealthDepletedHandler();

protected:
    /** Array of possible first attacks (random selection) */
    UPROPERTY(EditAnywhere, Category = "Combat AI|Moves")
    TArray<FName> FirstMoveVariants = {
        FName("EnemyAttack"),
        FName("EnemyAttack2"),
        FName("EnemyAttack3")
    };

public:
    /** Picks a random first attack (called by BTTask_EnemyAttack) */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    FName SelectRandomFirstMove();
    
    /** Checks if currently executing a move */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool IsExecutingMove() const;
    
    /** Gets min attack distance for BT decorators */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    float GetMinAttackDistance() const { return MinAttackDistance; }
    
    /** Gets decision engine for BT tasks */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    UCombatDecisionEngine* GetDecisionEngine() const { return CombatDecisionEngine; }
    
protected:
    // ==================== JUMP SYSTEM ====================
    
    /** If true, enemy has used double jump in current air time */
    bool bHasDoubleJumped = false;
    
    /** Min height difference to trigger jump toward player */
    UPROPERTY(EditAnywhere, Category = "AI Movement|Jump")
    float JumpHeightThreshold = 150.0f;

    // ON ENEMY DEATH
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
    void BP_OnEnemyDeath();
public:
    /** Called when enemy lands (resets jump flags) */
    virtual void Landed(const FHitResult& Hit) override;
    /** Notify game mode that enemy landed a hit */
    UFUNCTION(BlueprintCallable, Category = "Dojo Stats")
    void NotifyEnemyHit(float DamageDealt);

    /** Reset enemy's current combo counter */
    UFUNCTION(BlueprintCallable, Category = "Dojo Stats")
    void ResetEnemyCombo();
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dojo Stats")
    bool bShowDebug=false;
    void TestDirectDamage(float Damage);

    FTimerHandle TempDamageTestHandle;
};
