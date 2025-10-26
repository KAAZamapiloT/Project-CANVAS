// Fill out your copyright notice in the Description page of Project Settings.


// BTTask_EnemyAttack.cpp

#include "BTTask_EnemyAttack.h"
#include "AIController.h"
#include "EnemyCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include"CombatDecisionEngine.h"
#include"CombatStateComponent.h"
UBTTask_EnemyAttack::UBTTask_EnemyAttack()
{
    NodeName = "Execute Combat Decision";
    bNotifyTick = false; // We'll use montage delegates instead
}

EBTNodeResult::Type UBTTask_EnemyAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(OwnerComp.GetAIOwner()->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;
    
    // Just call the character's method
    Enemy->ExecuteAttack();
    
    return EBTNodeResult::Succeeded;
}
