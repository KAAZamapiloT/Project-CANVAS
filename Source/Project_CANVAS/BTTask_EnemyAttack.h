// Fill out your copyright notice in the Description page of Project Settings.

// BTTask_EnemyAttack.h
// Behavior tree task that picks an attack and executes via CombatAnimationComponent
// No Decision Engine - just direct montage execution

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "CombatAnimationComponent.h"
#include "BTTask_EnemyAttack.generated.h"

/**
 * UBTTask_EnemyAttack
 * 
 * Behavior tree task for enemy attacks.
 * Picks attack based on distance to player:
 * - Far: Dash attack
 * - Mid: Light attack
 * - Close: Heavy attack
 * 
 * Calls EnemyCharacter::ExecuteAttack() directly
 */
class UCombatDecisionEngine;

UCLASS()
class PROJECT_CANVAS_API UBTTask_EnemyAttack : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_EnemyAttack();

	/** Key to get target actor (usually player) */
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector TargetActorKey;

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	UBehaviorTreeComponent* CachedOwnerComp;
};