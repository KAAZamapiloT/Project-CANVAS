// Fill out your copyright notice in the Description page of Project Settings.

// HealthComponent.cpp
// Manages health, damage, and death for characters
// Complete implementation with stun, invincibility, and state safety

#include "HealthComponent.h"
#include "GameFramework/Character.h"
#include "DojoGameMode.h" 
UHealthComponent::UHealthComponent()
{
    // ✅ Enable tick for stun/invincibility countdown (IMPORTANT!)
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.01f; // 10ms updates for smooth countdown
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    // ✅ Initialize health to max
    Health = MaxHealth;
    StunDuration = 0.f;
    InvincibilityTimer = 0.f;

    // ✅ Validate owner
    if (!GetOwner())
    {
        UE_LOG(LogTemp, Error, TEXT("HealthComponent: Owner is null!"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("✅ HealthComponent initialized: %.1f / %.1f HP"), Health, MaxHealth);
}

void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // ✅ SAFETY CHECK: Validate operations
    if (!IsValidForOperations())
        return;

    // ✅ Tick stun duration countdown
    if (StunDuration > 0.f)
    {
        TickStun(DeltaTime);
    }

    // ✅ Tick invincibility frames countdown
    if (InvincibilityTimer > 0.f)
    {
        TickInvincibility(DeltaTime);
    }
}

void UHealthComponent::ApplyDamage(float Amount, EDamageType DamageType, FVector HitLocation)
{
    // ✅ SAFETY CHECK 1: Validate component state
    if (!IsValidForOperations())
    {
        UE_LOG(LogTemp, Warning, TEXT("HealthComponent: Not valid for operations"));
        return;
    }
    // ✅ SAFETY CHECK 2: Already dead
    if (!IsAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Cannot damage dead character"));
        return;
    }

    // ✅ SAFETY CHECK 3: Invalid damage amount
    if (Amount <= 0.f)
    {
        return;
    }

    // ✅ SAFETY CHECK 4: In invincibility frames
    if (IsInvincible())
    {
        UE_LOG(LogTemp, Log, TEXT("⚡ Damage blocked by invincibility frames!"));
        return;
    }
    // ✅ CHECK IF IN DOJO MODE - ADD THIS SECTION
    ADojoGameMode* DojoMode = Cast<ADojoGameMode>(GetWorld()->GetAuthGameMode());
    if (DojoMode)
    {
        // ✅ DOJO MODE: Track hit but don't apply damage
        float ActualDamage = Amount * (1.f - DamageReduction);
        
        UE_LOG(LogTemp, Warning, TEXT("🥋 [DOJO] Hit registered (%.1f damage) - No health lost"), ActualDamage);
        
        // Determine who got hit by checking tags
        AActor* Owner = GetOwner();
        if (Owner)
        {
            if (Owner->ActorHasTag("Player"))
            {
                DojoMode->RecordEnemyHit(ActualDamage); // Player got hit
            }
            else if (Owner->ActorHasTag("Enemy.Character"))
            {
                DojoMode->RecordPlayerHit(ActualDamage); // Enemy got hit
            }
        }

        // Still play hit reaction animation
        OnDamageTaken.Broadcast(ActualDamage, HitLocation);
        
        // ✅ EXIT EARLY - No damage in Dojo Mode
        return;
    }
    // Apply damage reduction (armor/defense system)
    float ActualDamage = Amount * (1.f - DamageReduction);

    // Apply damage and clamp
    Health = FMath::Clamp(Health - ActualDamage, 0.f, MaxHealth);

    // Start invincibility frames (prevents rapid hit spam)
    InvincibilityTimer = InvincibilityFrames;

    // Apply stun based on damage type
    if (DamageType == EDamageType::Electric)
    {
        // Electric damage stuns proportionally to damage amount
        ApplyStun(ActualDamage * 0.05f);
    }

    UE_LOG(LogTemp, Log, TEXT("💥 Damage: %.1f (type: %d), Health: %.1f / %.1f"),
        ActualDamage, (int32)DamageType, Health, MaxHealth);

    // Fire callbacks for visual feedback
    OnDamageTaken.Broadcast(ActualDamage, HitLocation);
    OnHealthChanged.Broadcast(Health, MaxHealth);

    // ✅ Check for death
    if (Health <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("💀 Character died!"));
        OnHealthDepleted.Broadcast();
    }
}

void UHealthComponent::Heal(float Amount)
{
    // ✅ SAFETY CHECK 1: Validate state
    if (!IsValidForOperations())
    {
        UE_LOG(LogTemp, Warning, TEXT("HealthComponent: Not valid for operations"));
        return;
    }

    // ✅ SAFETY CHECK 2: Cannot heal dead
    if (!IsAlive())
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ Cannot heal dead character"));
        return;
    }

    // ✅ SAFETY CHECK 3: Invalid heal amount
    if (Amount <= 0.f)
    {
        return;
    }

    // Apply heal and clamp to max
    Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);

    UE_LOG(LogTemp, Log, TEXT("🏥 Healed: %.1f HP, Health: %.1f / %.1f"),
        Amount, Health, MaxHealth);

    OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UHealthComponent::ApplyStun(float Duration)
{
    // ✅ SAFETY CHECK 1: Validate state
    if (!IsValidForOperations())
        return;

    // ✅ SAFETY CHECK 2: Invalid duration
    if (Duration <= 0.f)
        return;

    // ✅ SAFETY CHECK 3: Only stun if alive
    if (!IsAlive())
        return;

    StunDuration = Duration;
    UE_LOG(LogTemp, Warning, TEXT("⚡ Character stunned for %.2f seconds"), Duration);
}

void UHealthComponent::ResetHealth()
{
    // ✅ Reset all state variables
    Health = MaxHealth;
    StunDuration = 0.f;
    InvincibilityTimer = 0.f;

    UE_LOG(LogTemp, Log, TEXT("🔄 Health reset to %.1f"), MaxHealth);
    OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UHealthComponent::Kill()
{
    // ✅ Set health to 0 and clear timers
    Health = 0.f;
    StunDuration = 0.f;
    InvincibilityTimer = 0.f;

    OnHealthDepleted.Broadcast();
    UE_LOG(LogTemp, Warning, TEXT("💀 Character killed"));
}

EHealthState UHealthComponent::GetHealthState() const
{
    // Priority order: Dead > Stunned > Critical > Damaged > Healthy

    if (!IsAlive())
        return EHealthState::Dead;

    if (IsStunned())
        return EHealthState::Stunned;

    if (GetHealthPercent() <= CriticalHealthThreshold)
        return EHealthState::Critical;

    if (GetHealthPercent() < 1.f)
        return EHealthState::Damaged;

    return EHealthState::Healthy;
}

bool UHealthComponent::IsValidForOperations() const
{
    // ✅ Comprehensive validation

    // Check component is valid
    if (!IsValidLowLevel())
        return false;

    // Check owner exists
    if (!GetOwner())
        return false;

    // Check owner is valid
    if (!GetOwner()->IsValidLowLevel())
        return false;

    return true;
}

void UHealthComponent::TickStun(float DeltaTime)
{
    // ✅ Countdown stun duration
    StunDuration -= DeltaTime;

    if (StunDuration <= 0.f)
    {
        StunDuration = 0.f;
        OnStunRecovered.Broadcast();
        UE_LOG(LogTemp, Log, TEXT("✅ Stun ended"));
    }
}

void UHealthComponent::TickInvincibility(float DeltaTime)
{
    // ✅ Countdown invincibility frames
    InvincibilityTimer -= DeltaTime;

    if (InvincibilityTimer < 0.f)
    {
        InvincibilityTimer = 0.f;
    }
}
