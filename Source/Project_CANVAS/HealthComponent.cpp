// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthComponent.h"


UHealthComponent::UHealthComponent()
{
  PrimaryComponentTick.bCanEverTick = false; // Disable tick
}

void UHealthComponent::BeginPlay()
{
  Super::BeginPlay();
  Health = MaxHealth; // Initialize to full
}

void UHealthComponent::ApplyDamage(float Amount)
{
  if (Amount <= 0.f || Health <= 0.f) return;
  
  Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);
  
  if (Health <= 0.f)
  {
    OnHealthDepleted.Broadcast();
  }
  else
  {
    OnHealthChanged.Broadcast(Health, MaxHealth);
  }
}

void UHealthComponent::Heal(float Amount)
{
  if (Amount <= 0.f || Health <= 0.f) return;
  
  Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
  OnHealthChanged.Broadcast(Health, MaxHealth);
}

