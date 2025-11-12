// BTTask_SmartAttack.cpp
#include "BTTask_SmartAttack.h"
#include "EnemyCharacter.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "CombatStateComponent.h"
#include "CombatDecisionEngine.h"
#include "CombatAnimationComponent.h"

UBTTask_SmartAttack::UBTTask_SmartAttack()
{
    NodeName = "Smart Attack (Combo)";
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_SmartAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = Cast<AAIController>(OwnerComp.GetAIOwner());
    if (!AIController)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BTTask_SmartAttack: No AIController"));
        return EBTNodeResult::Failed;
    }

    AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
    if (!Enemy)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BTTask_SmartAttack: No Enemy pawn"));
        return EBTNodeResult::Failed;
    }

    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    if (!Blackboard)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ BTTask_SmartAttack: No Blackboard"));
        return EBTNodeResult::Failed;
    }

    // ✅ STEP 1: Get LastMove from Blackboard
    FName LastMove = Blackboard->GetValueAsName("LastMoveExecuted");
    
    if (LastMove.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ [BT] SmartAttack: No previous move - skipping combo"));
        return EBTNodeResult::Failed;
    }

    UE_LOG(LogTemp, Warning, TEXT("🧠 [BT] SmartAttack: Using LastMove=%s for combo prediction"), 
           *LastMove.ToString());

    // ✅ STEP 2: Check if still in range
    float Distance = Blackboard->GetValueAsFloat("DistanceToPlayer");
    float MinAttackDistance = Enemy->GetMinAttackDistance();
    
    if (Distance > MinAttackDistance)
    {
        UE_LOG(LogTemp, Display, TEXT("⏸️ [BT] SmartAttack: Out of range (%.1f > %.1f)"), 
               Distance, MinAttackDistance);
        return EBTNodeResult::Failed;
    }

    // ✅ STEP 3: Get components
    UCombatStateComponent* StateComp = Enemy->FindComponentByClass<UCombatStateComponent>();
    UCombatDecisionEngine* DecisionEngine = Enemy->GetDecisionEngine();
    UCombatAnimationComponent* AnimComp = Enemy->FindComponentByClass<UCombatAnimationComponent>();
    
    if (!StateComp || !DecisionEngine || !AnimComp)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [BT] SmartAttack: Missing components"));
        return EBTNodeResult::Failed;
    }

    // ✅ STEP 4: Build context with LastMove (enables combo prediction)
    FContextVector Context = StateComp->BuildContext(LastMove);
    
    UE_LOG(LogTemp, Display, TEXT("🔍 [BT] SmartAttack: Context built with LastMove=%s"), 
           *LastMove.ToString());

    // ✅ STEP 5: Let Decision Engine predict follow-up
    FActionCommand FollowUp = DecisionEngine->DecideNextMove(Context);
    
    if (FollowUp.MoveIdentifier.IsNone())
    {
        UE_LOG(LogTemp, Display, TEXT("⏸️ [BT] SmartAttack: No valid follow-up - combo ended"));
        return EBTNodeResult::Failed;
    }

    UE_LOG(LogTemp, Warning, TEXT("⚡ [BT] SmartAttack: Predicted follow-up: %s"), 
           *FollowUp.MoveIdentifier.ToString());

    // ✅ STEP 6: Execute follow-up
    AnimComp->ExecuteActionCommand(FollowUp);
    StateComp->StartCooldown(FollowUp.MoveIdentifier, 0.5f);
    
    // Update LastMove for potential next combo
    Blackboard->SetValueAsName("LastMoveExecuted", FollowUp.MoveIdentifier);
    
    // Return InProgress - will complete when montage ends
    return EBTNodeResult::InProgress;
}

void UBTTask_SmartAttack::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
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
        UE_LOG(LogTemp, Display, TEXT("✅ [BT] SmartAttack: Montage completed"));
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}
