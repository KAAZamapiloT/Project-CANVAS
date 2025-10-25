// Fill out your copyright notice in the Description page of Project Settings.

// EnemyCharacter.h
// Enemy uses behavior tree logic, NOT Decision Engine
// Player uses Decision Engine for assistive combos

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Damagable.h"

#include"EnemyCharacter.generated.h"

class UHealthComponent;
class UCombatAnimationComponent;
class UBehaviorTree;
class UAnimMontage;

/**
 * AEnemyCharacter
 * 
 * Enemy for 2.5D fighting game.
 * Uses traditional behavior tree AI (patrol, chase, attack)
 * Attacks via CombatAnimationComponent but picks montages directly
 * Player uses Decision Engine; enemy does not
 */
UCLASS()
class PROJECT_CANVAS_API AEnemyCharacter : public ACharacter, public IDamagable
{
    GENERATED_BODY()

public:
    AEnemyCharacter();


    // =====================================
    // COMBAT COMPONENTS
    // =====================================
    
    /** Health management */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UHealthComponent* HealthComp;

    /** Animation execution (plays montages from behavior tree) */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
    UCombatAnimationComponent* CombatAnimComp;

    // =====================================
    // ENEMY ATTACK MONTAGES (assigned in editor)
    // Behavior tree picks one randomly or based on distance
    // =====================================
    
    /** Light attack montage */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat")
    UAnimMontage* LightAttackMontage;

    /** Heavy attack montage */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat")
    UAnimMontage* HeavyAttackMontage;

    /** Dash/gap-closer montage */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Combat")
    UAnimMontage* DashMontage;

    // =====================================
    // AI COMPONENTS
    // =====================================
    
    /** Behavior tree for AI logic */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
    UBehaviorTree* BehaviorTree;

    virtual void BeginPlay() override;

public:
    // =====================================
    // IDAMAGABLE INTERFACE
    // =====================================
    
    virtual void ReceiveDamage_Implementation(const FDamageSpec& Spec) override;
    virtual bool IsAlive_Implementation() const override;

    // =====================================
    // BEHAVIOR TREE CALLABLE FUNCTIONS
    // Called from BTTask_EnemyAttack
    // =====================================
    
    /**
     * Execute a specific attack by montage reference
     * Behavior tree picks the montage based on distance/state
     * 
     * @param AttackMontage - The montage to play
     * @param Damage - Damage to deal on hit
     * @param Stun - Stun duration on hit
     */
    UFUNCTION(BlueprintCallable, Category="Combat")
    void ExecuteAttack(UAnimMontage* AttackMontage, float Damage, float Stun);

protected:
    // =====================================
    // COMBAT DELEGATES
    // =====================================
    
    UFUNCTION()
    void OnMoveCompleted(FName CompletedMove);

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
    
    void PerformHitDetection(float Damage, float Stun);
};
