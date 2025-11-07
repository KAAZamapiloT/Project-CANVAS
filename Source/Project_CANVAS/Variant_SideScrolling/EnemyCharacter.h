// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "CombatData.h"
#include "Components/CapsuleComponent.h"
#include "Damagable.h"
#include "EnemyCharacter.generated.h"

// Forward declarations
class UAnimMontage;
class UHealthComponent;
class UCombatAnimationComponent;
class UCombatStateComponent;
class UCombatDecisionEngine;

/// @enum EEnemyState
/// @brief Enemy AI state enumeration
UENUM(BlueprintType)
enum class EEnemyState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Attacking   UMETA(DisplayName = "Attacking"),
    Stunned     UMETA(DisplayName = "Stunned"),
    Dead        UMETA(DisplayName = "Dead")
};

/// @class AEnemyCharacter
/// @brief AI-controlled combat character
/// 
/// @details
/// **Architecture:**
/// - CombatStateComponent: Builds FContextVector from game state
/// - CombatDecisionEngine: Analyzes context, selects best move (black box)
/// - CombatAnimationComponent: Executes animation
/// - IDamagable: Receives damage when hit
///
/// **Combat Flow:**
/// MakeCombatDecision() → CombatDecisionEngine::DecideNextMove(Context)
///                    → ExecuteMove() → Animation → Damage on hit
///
/// **Note:** For enemy, PlayerCharacter is set in constructor (the opponent)
UCLASS()
class PROJECT_CANVAS_API AEnemyCharacter : public ACharacter, public IDamagable
{
    GENERATED_BODY()

public:
    /// @brief Constructor - initialize with opponent reference
    /// @details Sets PlayerCharacter to the player pawn (opponent)
    AEnemyCharacter();

    /// @brief Initialize enemy on level start
    virtual void BeginPlay() override;

    /// @brief Per-frame update
    /// @param DeltaTime Time elapsed since last frame (seconds)
    virtual void Tick(float DeltaTime) override;

    /// @brief Cleanup when level unloads
    /// @param EndPlayReason Why this actor is ending play
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ========== PROPERTIES ==========

    /// @brief Maximum health points
    /// @default 100.0f
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Config")
    float MaxHealth = 100.0f;

    /// @brief Time between AI decision cycles (seconds)
    /// @default 1.5f
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Config")
    float DecisionInterval = 1.5f;

    /// @brief Minimum distance to player before attacking (centimeters)
    /// @default 100.0f
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Config")
    float MinAttackDistance = 100.0f;

    /// @brief DataTable containing enemy-specific move definitions
    /// @details Structure: FActionCommand (move name, animation, damage, stun)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Config")
    class UDataTable* EnemyMoveDataTable = nullptr;

    // ========== COMPONENTS ==========

    /// @brief Health tracking component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    class UHealthComponent* HealthComponent = nullptr;

    /// @brief Animation execution component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UCombatAnimationComponent* CombatAnimationComponent = nullptr;

    /// @brief Combat context analyzer (builds FContextVector)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UCombatStateComponent* CombatStateComponent = nullptr;

    /// @brief AI decision engine (black box - analyzes context, picks move)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UCombatDecisionEngine* CombatDecisionEngine = nullptr;

    // ========== COMBAT FUNCTIONS ==========

    /// @brief Start enemy AI combat loop
    UFUNCTION(BlueprintCallable, Category="Combat")
    void StartCombatBehavior();

    /// @brief Stop enemy AI combat loop
    UFUNCTION(BlueprintCallable, Category="Combat")
    void StopCombatBehavior();

    /// @brief Make AI decision and execute move
    /// @details
    /// Queries CombatDecisionEngine with context from CombatStateComponent.
    /// Decision engine internally ranks all available moves and returns best one.
    UFUNCTION(BlueprintCallable, Category="Combat")
    void MakeCombatDecision();

    /// @brief Execute a specific move by name
    /// @param MoveName Row name in EnemyMoveDataTable
    UFUNCTION(BlueprintCallable, Category="Combat")
    void ExecuteMove(FName MoveName);

    /// @brief Called when attack montage finishes
    UFUNCTION(BlueprintCallable, Category="Combat")
    void OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted);

    // ========== STATE QUERIES ==========

    /// @brief Get current enemy state
    UFUNCTION(BlueprintPure, Category="Combat")
    EEnemyState GetEnemyState() const;

    /// @brief Get distance from enemy to opponent
    UFUNCTION(BlueprintPure, Category="Combat")
    float GetDistanceToPlayer() const;

    /// @brief Get direction to opponent (-1 = left, 1 = right)
    UFUNCTION(BlueprintPure, Category="Combat")
    float GetPlayerDirection() const;

    /// @brief Check if enemy can attack opponent now
    UFUNCTION(BlueprintPure, Category="Combat")
    bool CanAttackPlayer() const;

    // ========== IDAMAGABLE INTERFACE ==========

    /// @brief Receive damage when hit
    /// @param Spec Damage specification (amount, location, causer)
    virtual void ReceiveDamage_Implementation(const FDamageSpec& Spec) override;

    /// @brief Check if enemy is alive
    virtual bool IsAlive_Implementation() const override;

protected:
    // ========== INTERNAL STATE ==========

    /// @brief Opponent reference (the player or target)
    /// @details Set in constructor to player pawn
    UPROPERTY()
    class ACharacter* PlayerCharacter = nullptr;

    /// @brief Timer handle for combat decision loop
    FTimerHandle DecisionTimerHandle;

    bool bIsInCombat = false;
    bool bIsExecutingMove = false;

    // ========== INITIALIZATION ==========

    /// @brief Setup animation references
    void SetupAnimations();

    /// @brief Bind combat delegates
    void BindCombatDelegates();

    /// @brief Create and initialize all combat components
    void InitializeCombatComponents();

    // ========== DATA ACCESS ==========

    /// @brief Look up move data in EnemyMoveDataTable
    /// @param MoveName Row key to find
    /// @return Pointer to FActionCommand if found, nullptr otherwise
    FActionCommand* GetMoveFromDataTable(FName MoveName);
};
