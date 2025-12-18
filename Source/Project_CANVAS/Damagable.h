// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Damagable.generated.h"

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
USTRUCT(BlueprintType)
struct FDamageSpec {
  GENERATED_BODY()
  UPROPERTY(EditAnywhere, BlueprintReadWrite) float Amount = 0.f;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) EDamageType DamageType=EDamageType::Physical;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector HitLocation = FVector::ZeroVector;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector HitNormal = FVector::UpVector;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HitBone = NAME_None;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) AController* InstigatorController = nullptr;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) AActor* DamageCauser = nullptr;
};

UINTERFACE(BlueprintType) // ADD THIS
class UDamagable : public UInterface
{
  GENERATED_BODY()
};

class PROJECT_CANVAS_API IDamagable
{
  GENERATED_BODY()
public:
  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Damage")
  void ReceiveDamage(const FDamageSpec& Spec);

  UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Damage")
  bool IsAlive() const;
};
