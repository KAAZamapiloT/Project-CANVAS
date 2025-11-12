#include "BTService_UpdateCombatState.h"
#include "EnemyCharacter.h"
#include "HealthComponent.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

UBTService_UpdateCombatState::UBTService_UpdateCombatState()
{
	NodeName = "Update Combat State";
	Interval = 0.2f; // Update 5 times per second
	RandomDeviation = 0.05f;
}

void UBTService_UpdateCombatState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = Cast<AAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
		return;

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(AIController->GetPawn());
	if (!Enemy)
		return;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
		return;

	// ✅ Get player
	ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (Player)
	{
		Blackboard->SetValueAsObject(PlayerActorKey.SelectedKeyName, Player);

		// ✅ Calculate X-axis distance (sidescroller)
		float DistanceX = FMath::Abs(Enemy->GetActorLocation().X - Player->GetActorLocation().X);
		Blackboard->SetValueAsFloat(DistanceToPlayerKey.SelectedKeyName, DistanceX);
	}

	// ✅ Update health percentage
	if (Enemy->HealthComponent)
	{
		float HealthPercent = (Enemy->HealthComponent->Health / Enemy->HealthComponent->MaxHealth) * 100.0f;
		Blackboard->SetValueAsFloat(HealthPercentageKey.SelectedKeyName, HealthPercent);

		// ✅ Update stunned state
		Blackboard->SetValueAsBool(IsStunnedKey.SelectedKeyName, Enemy->HealthComponent->IsStunned());
	}

	// ✅ Can attack check
	bool bCanAttack = Enemy->CanAttackPlayer();
	Blackboard->SetValueAsBool(CanAttackKey.SelectedKeyName, bCanAttack);
}
