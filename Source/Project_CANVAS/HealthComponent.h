// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHealthDepleted);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECT_CANVAS_API UHealthComponent : public UActorComponent
{
  GENERATED_BODY()
public:
  UHealthComponent();

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health")
  float MaxHealth = 100.f;

  UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Health")
  float Health = 0.f;

  UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Health")
  float StunDuration = 0.f;

  // Delegates
  UPROPERTY(BlueprintAssignable, Category="Health")
  FOnHealthChanged OnHealthChanged;

  UPROPERTY(BlueprintAssignable, Category="Health")
  FOnHealthDepleted OnHealthDepleted;

  // Public methods
  UFUNCTION(BlueprintCallable, Category="Health")
  void ApplyDamage(float Amount);

  UFUNCTION(BlueprintCallable, Category="Health")
  void Heal(float Amount);

  UFUNCTION(BlueprintPure, Category="Health")
  float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

  UFUNCTION(BlueprintPure, Category="Health")
  bool IsAlive() const { return Health > 0.f; }

protected:
  virtual void BeginPlay() override;
};
