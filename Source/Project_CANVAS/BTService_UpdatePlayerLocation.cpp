#include "BTService_UpdatePlayerLocation.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

UBTService_UpdatePlayerLocation::UBTService_UpdatePlayerLocation()
{
	NodeName = "Update Player Location";
	Interval = 0.5f; // Update twice per second
	RandomDeviation = 0.1f;
}

void UBTService_UpdatePlayerLocation::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	AAIController* AIController = Cast<AAIController>(OwnerComp.GetAIOwner());
	if (!AIController)
		return;

	APawn* EnemyPawn = AIController->GetPawn();
	if (!EnemyPawn)
		return;

	// Get player
	ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!Player)
		return;

	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
		return;

	// Update blackboard
	Blackboard->SetValueAsObject(PlayerActorKey.SelectedKeyName, Player);

	float Distance = FVector::Dist(EnemyPawn->GetActorLocation(), Player->GetActorLocation());
	Blackboard->SetValueAsFloat(DistanceToPlayerKey.SelectedKeyName, Distance);
}
