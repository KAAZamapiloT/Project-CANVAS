// Fill out your copyright notice in the Description page of Project Settings.

// HealthComponent.h
// Manages health, damage, and death for characters

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

/** Delegate fired when health changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);

/** Delegate fired when health reaches zero */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthDepleted);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECT_CANVAS_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHealthComponent();

    /** Maximum health value */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health")
    float MaxHealth = 100.f;

    /** Current health value */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
    float Health = 0.f;

    /** Duration this character is stunned (set by damage, ticks down) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health")
    float StunDuration = 0.f;

    /** Fired when health changes */
    UPROPERTY(BlueprintAssignable, Category="Health")
    FOnHealthChanged OnHealthChanged;

    /** Fired when health reaches zero */
    UPROPERTY(BlueprintAssignable, Category="Health")
    FOnHealthDepleted OnHealthDepleted;

    /**
     * Apply damage to this component
     * Clamps health to [0, MaxHealth] and fires delegates
     * 
     * @param Amount - Damage to apply (positive value)
     */
    UFUNCTION(BlueprintCallable, Category="Health")
    void ApplyDamage(float Amount);

    /**
     * Heal this component
     * Clamps health to [0, MaxHealth] and fires delegates
     * 
     * @param Amount - Health to restore (positive value)
     */
    UFUNCTION(BlueprintCallable, Category="Health")
    void Heal(float Amount);

    /** Get health as a 0-1 percentage */
    UFUNCTION(BlueprintPure, Category="Health")
    float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

    /** Check if this component's health is above zero */
    UFUNCTION(BlueprintPure, Category="Health")
    bool IsAlive() const { return Health > 0.f; }

protected:
    virtual void BeginPlay() override;
};

