#include "BTTask_EnemyAttack.h"
#include "EnemyCharacter.h"
#include "AIController.h"

UBTTask_EnemyAttack::UBTTask_EnemyAttack()
{
    NodeName = "Enemy Attack";
    bCreateNodeInstance = false;
}

EBTNodeResult::Type UBTTask_EnemyAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // ✅ Get AI controller
    AAIController* AIController = Cast<AAIController>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        UE_LOG(LogTemp, Error, TEXT("BTTask_EnemyAttack: No AI Controller"));
        return EBTNodeResult::Failed;
    }

    // ✅ Get enemy character
    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    if (!Enemy)
    {
        UE_LOG(LogTemp, Error, TEXT("BTTask_EnemyAttack: No Enemy Character"));
        return EBTNodeResult::Failed;
    }

    // ✅ Call enemy's attack logic (uses CombatDecisionEngine internally)
    Enemy->MakeCombatDecision();

    UE_LOG(LogTemp, Log, TEXT("✅ BTTask_EnemyAttack: Executed"));

    return EBTNodeResult::Succeeded;
}
