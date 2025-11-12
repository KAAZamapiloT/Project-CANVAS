#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateCombatState.generated.h"

/**
 * Continuously updates combat-related blackboard keys
 */
UCLASS()
class PROJECT_CANVAS_API UBTService_UpdateCombatState : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdateCombatState();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// Blackboard keys
	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector PlayerActorKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector DistanceToPlayerKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector HealthPercentageKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector CanAttackKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector IsStunnedKey;
};
