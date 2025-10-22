// Fill out your copyright notice in the Description page of Project Settings.


// HealthComponent.cpp

#include "HealthComponent.h"

UHealthComponent::UHealthComponent()
{
  // Disable tick - health updates are event-driven
  PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
  Super::BeginPlay();
    
  // Initialize health to max
  Health = MaxHealth;
    
  UE_LOG(LogTemp, Log, TEXT("HealthComponent initialized: %.1f / %.1f"), Health, MaxHealth);
}

void UHealthComponent::ApplyDamage(float Amount)
{
  // Validate input
  if (Amount <= 0.f || Health <= 0.f)
  {
    return;  // Already dead or invalid damage
  }
    
  // Apply damage and clamp
  Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);
    
  // Check for death
  if (Health <= 0.f)
  {
    UE_LOG(LogTemp, Warning, TEXT("Health depleted!"));
    OnHealthDepleted.Broadcast();
  }
  else
  {
    UE_LOG(LogTemp, Log, TEXT("Took %.1f damage, health now %.1f / %.1f"), 
           Amount, Health, MaxHealth);
    OnHealthChanged.Broadcast(Health, MaxHealth);
  }
}

void UHealthComponent::Heal(float Amount)
{
  // Validate input
  if (Amount <= 0.f || Health <= 0.f)
  {
    return;  // Dead or invalid heal
  }
    
  // Apply heal and clamp
  Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
    
  UE_LOG(LogTemp, Log, TEXT("Healed %.1f, health now %.1f / %.1f"), 
         Amount, Health, MaxHealth);
  OnHealthChanged.Broadcast(Health, MaxHealth);
}


