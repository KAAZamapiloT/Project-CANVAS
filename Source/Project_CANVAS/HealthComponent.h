// Fill out your copyright notice in the Description page of Project Settings.

// HealthComponent.h
// Manages health, damage, and death for characters
// Complete health system with stun, invincibility, and state safety

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

// ============ ENUMS ============

UENUM(BlueprintType)
enum class EDamageType : uint8
{
    Physical     UMETA(DisplayName = "Physical"),
    Fire         UMETA(DisplayName = "Fire"),
    Ice          UMETA(DisplayName = "Ice"),
    Electric     UMETA(DisplayName = "Electric"),
    Poison       UMETA(DisplayName = "Poison")
};

UENUM(BlueprintType)
enum class EHealthState : uint8
{
    Healthy      UMETA(DisplayName = "Healthy"),
    Damaged      UMETA(DisplayName = "Damaged"),
    Critical     UMETA(DisplayName = "Critical"),
    Dead         UMETA(DisplayName = "Dead"),
    Stunned      UMETA(DisplayName = "Stunned")
};

// ============ DELEGATES ============

/** Fired when health changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);

/** Fired when health reaches zero */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthDepleted);

/** Fired when damage is taken (for visual feedback) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamageTaken, float, DamageAmount, FVector, HitLocation);

/** Fired when character recovers from stun */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStunRecovered);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
// ============ COMPONENT ============

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECT_CANVAS_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHealthComponent();

    // ========== PROPERTIES ==========

    /** Maximum health value */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Config")
    float MaxHealth = 100.f;

    /** Current health value */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health|State")
    float Health = 0.f;

    /** Duration this character is stunned (set by damage, ticks down) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Config")
    float StunDuration = 0.f;

    /** Health threshold for "critical" state (0-1 percentage) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Config", meta=(ClampMin="0", ClampMax="1"))
    float CriticalHealthThreshold = 0.25f;

    /** Invincibility frames after taking damage (prevents rapid hits) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Config")
    float InvincibilityFrames = 0.5f;

    /** Damage reduction factor (0-1, where 0.2 = 20% reduction) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health|Config")
    float DamageReduction = 0.1f;

    /** Time remaining in invincibility state */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health|State")
    float InvincibilityTimer = 0.f;

    // ========== DELEGATES ==========

    /** Fired when health changes */
    UPROPERTY(BlueprintAssignable, Category="Health|Events")
    FOnHealthChanged OnHealthChanged;

    /** Fired when health reaches zero */
    UPROPERTY(BlueprintAssignable, Category="Health|Events")
    FOnHealthDepleted OnHealthDepleted;

    /** Fired when damage is taken */
    UPROPERTY(BlueprintAssignable, Category="Health|Events")
    FOnDamageTaken OnDamageTaken;

    /** Fired when stun ends */
    UPROPERTY(BlueprintAssignable, Category="Health|Events")
    FOnStunRecovered OnStunRecovered;

    // ========== DAMAGE & HEALING ==========

    /**
     * Apply damage to this component
     * Applies damage reduction, starts invincibility frames
     * 
     * @param Amount - Damage to apply (positive value)
     * @param DamageType - Type of damage for effects
     * @param HitLocation - World location of hit for feedback
     */
    UFUNCTION(BlueprintCallable, Category="Health|Damage")
    void ApplyDamage(float Amount, EDamageType DamageType = EDamageType::Physical, FVector HitLocation = FVector::ZeroVector);

    /**
     * Heal this component
     * Cannot heal if dead, does not interrupt stun
     * 
     * @param Amount - Health to restore (positive value)
     */
    UFUNCTION(BlueprintCallable, Category="Health|Healing")
    void Heal(float Amount);

    /**
     * Apply stun effect to character
     * Prevents combat actions during stun
     * 
     * @param Duration - How long to stun (seconds)
     */
    UFUNCTION(BlueprintCallable, Category="Health|Effects")
    void ApplyStun(float Duration);

    /**
     * Reset health to maximum
     * Used for respawn or testing
     */
    UFUNCTION(BlueprintCallable, Category="Health|Management")
    void ResetHealth();

    /**
     * Instantly kill character
     */
    UFUNCTION(BlueprintCallable, Category="Health|Management")
    void Kill();

    // ========== QUERIES ==========

    /** Check if currently alive (health > 0) */
    UFUNCTION(BlueprintPure, Category="Health|Query")
    bool IsAlive() const { return Health > 0.f; }

    /** Check if currently stunned */
    UFUNCTION(BlueprintPure, Category="Health|Query")
    bool IsStunned() const { return StunDuration > 0.f; }

    /** Check if in invincibility frames */
    UFUNCTION(BlueprintPure, Category="Health|Query")
    bool IsInvincible() const { return InvincibilityTimer > 0.f; }

    /** Get health as a 0-1 percentage */
    UFUNCTION(BlueprintPure, Category="Health|Query")
    float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

    /** Get current health state (Healthy/Damaged/Critical/Dead) */
    UFUNCTION(BlueprintPure, Category="Health|Query")
    EHealthState GetHealthState() const;

    /** Check if at critical health threshold */
    UFUNCTION(BlueprintPure, Category="Health|Query")
    bool IsCritical() const { return GetHealthPercent() <= CriticalHealthThreshold && IsAlive(); }

    /** Validate component is ready for operations */
    UFUNCTION(BlueprintPure, Category="Health|Query")
    bool IsValidForOperations() const;
public:
    /** Enable Dojo training mode (stats only, no actual damage) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Dojo")
    bool bDojoMode = false;

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    /** Internal tick for stun duration countdown */
    void TickStun(float DeltaTime);

    /** Internal tick for invincibility frame countdown */
    void TickInvincibility(float DeltaTime);
    // In HealthComponent.h
public:
    /** Called when health reaches 0 */
   
    UPROPERTY(BlueprintAssignable, Category = "Health")
    FOnDeath OnDeath;
     UFUNCTION(BlueprintCallable, Category="Health")
    void Respawn();
    
};
