// Fill out your copyright notice in the Description page of Project Settings.

// EnemyCharacter.h
// Enemy character for 2.5D side-scrolling fighting game
// Adapted from your 3D horror game AEvilWomen character

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Damagable.h"  // Your IDamagable interface
#include "EnemyCharacter.generated.h"

// Forward declarations
class UHealthComponent;
class UCombatAnimationComponent;
class UBehaviorTree;
class UAnimMontage;

/**
 * AEnemyCharacter
 * 
 * Enemy for 2.5D side-scrolling fighting game.
 * Key differences from 3D horror game enemy:
 * - Plane constraint locks movement to X-Z plane (side-scroller zone)
 * - Uses Decision Engine for combat instead of random montages
 * - Implements IDamagable for unified damage handling
 * - AI queries CombatDecisionEngine at intervals instead of perception-based attacks
 */
UCLASS()
class PROJECT_CANVAS_API AEnemyCharacter : public ACharacter, public IDamagable
{
    GENERATED_BODY()

public:
    AEnemyCharacter();

protected:
    // =====================================
    // COMBAT COMPONENTS
    // =====================================
    
    /** Health management */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UHealthComponent* HealthComp;

    /** Animation execution (plays montages from Decision Engine) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UCombatAnimationComponent* CombatAnimComp;

    /** Combat montages (populated by Decision Engine from DataTable) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat")
    TArray<UAnimMontage*> AttackMontages;

    // =====================================
    // AI COMPONENTS (from your AEvilWomen)
    // =====================================
    
    /** Behavior tree for AI logic */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    UBehaviorTree* BehaviorTree;

    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // =====================================
    // IDAMAGABLE INTERFACE
    // =====================================
    
    /**
     * Receive damage from player attacks
     * Routes to HealthComponent like your old IIA_Damageable::Damage()
     */
    virtual void ReceiveDamage_Implementation(const FDamageSpec& Spec) override;

    /**
     * Check if enemy is alive
     */
    virtual bool IsAlive_Implementation() const override;

protected:
    // =====================================
    // COMBAT DELEGATES (from CombatAnimationComponent)
    // =====================================
    
    /** Called when attack montage ends */
    UFUNCTION()
    void OnMoveCompleted(FName CompletedMove);

    /** Called when hit notify fires during attack */
    UFUNCTION()
    void OnHitWindowActive(float Damage, float Stun);

    // =====================================
    // HEALTH DELEGATES
    // =====================================
    
    UFUNCTION()
    void OnHealthChanged(float Current, float Max);

    UFUNCTION()
    void OnDeath();

    // =====================================
    // COMBAT HELPERS
    // =====================================
    
    /**
     * Perform hit detection during attack
     * Uses sphere trace like your old AttackTrace() line trace
     */
    void PerformHitDetection(float Damage, float Stun);
};
