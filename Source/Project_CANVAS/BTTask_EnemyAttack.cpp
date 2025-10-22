// Fill out your copyright notice in the Description page of Project Settings.


// BTTask_EnemyAttack.cpp

#include "BTTask_EnemyAttack.h"
#include "AIController.h"
#include "EnemyCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_EnemyAttack::UBTTask_EnemyAttack()
{
    NodeName = "Enemy Attack";
}

EBTNodeResult::Type UBTTask_EnemyAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // Get AI controller and enemy character
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    if (!Enemy)
    {
        return EBTNodeResult::Failed;
    }

    // Check if already attacking
    if (Enemy->CombatAnimComp && Enemy->CombatAnimComp->IsExecutingMove())
    {
        return EBTNodeResult::Failed;
    }

    // Get target actor from blackboard
    UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
    AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(TargetActorKey.SelectedKeyName));
    
    if (!TargetActor)
    {
        return EBTNodeResult::Failed;
    }

    // Calculate distance to target
    float Distance = FVector::Dist(Enemy->GetActorLocation(), TargetActor->GetActorLocation());

    // =====================================
    // PICK ATTACK BASED ON DISTANCE
    // =====================================
    
    UAnimMontage* SelectedMontage = nullptr;
    float SelectedDamage = 0.f;

    if (Distance > DashDistance)
    {
        // Far: Dash attack to close gap
        SelectedMontage = Enemy->DashMontage;
        SelectedDamage = DashDamage;
        UE_LOG(LogTemp, Log, TEXT("Enemy picked: Dash attack (distance: %.1f)"), Distance);
    }
    else if (Distance < HeavyDistance)
    {
        // Close: Heavy attack for big damage
        SelectedMontage = Enemy->HeavyAttackMontage;
        SelectedDamage = HeavyDamage;
        UE_LOG(LogTemp, Log, TEXT("Enemy picked: Heavy attack (distance: %.1f)"), Distance);
    }
    else
    {
        // Mid-range: Light attack
        SelectedMontage = Enemy->LightAttackMontage;
        SelectedDamage = LightDamage;
        UE_LOG(LogTemp, Log, TEXT("Enemy picked: Light attack (distance: %.1f)"), Distance);
    }

    // Validate montage is assigned
    if (!SelectedMontage)
    {
        UE_LOG(LogTemp, Error, TEXT("Enemy attack montage not assigned in Blueprint!"));
        return EBTNodeResult::Failed;
    }

    // =====================================
    // EXECUTE ATTACK VIA ENEMY CHARACTER
    // =====================================
    
    Enemy->ExecuteAttack(SelectedMontage, SelectedDamage, StunDuration);

    return EBTNodeResult::Succeeded;
}
