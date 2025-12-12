// BTTask_EnemyAttack.cpp
#include "BTTask_EnemyAttack.h"
#include "EnemyCharacter.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_EnemyAttack::UBTTask_EnemyAttack()
{
    NodeName = "Enemy Attack (Random)";
    bNotifyTick = true; // Monitor montage completion
}

EBTNodeResult::Type UBTTask_EnemyAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = Cast<AAIController>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BTTask_EnemyAttack: No AIController"));
        return EBTNodeResult::Failed;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    if (!Enemy)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BTTask_EnemyAttack: No Enemy pawn"));
        return EBTNodeResult::Failed;
    }

    // ✅ STEP 1: Select random first attack
    FName RandomMove = Enemy->SelectRandomFirstMove();
    
   // UE_LOG(LogTemp, Warning, TEXT("🎲 [BT] EnemyAttack: Selected %s"), *RandomMove.ToString());

    // ✅ STEP 2: Execute the move
    Enemy->ExecuteMove(RandomMove);
    
    // ✅ STEP 3: Store in Blackboard for SmartAttack to use
    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    if (Blackboard)
    {
        Blackboard->SetValueAsName("LastMoveExecuted", RandomMove);
        UE_LOG(LogTemp, Display, TEXT("✅ [BT] Stored LastMove: %s"), *RandomMove.ToString());
    }

    // Return InProgress - will complete when montage ends
    return EBTNodeResult::InProgress;
}

void UBTTask_EnemyAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    AAIController* AIController = Cast<AAIController>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    if (!Enemy)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // ✅ CHECK: Wait for montage to complete
    if (!Enemy->IsExecutingMove())
    {
        UE_LOG(LogTemp, Display, TEXT("✅ [BT] EnemyAttack: Montage completed"));
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
