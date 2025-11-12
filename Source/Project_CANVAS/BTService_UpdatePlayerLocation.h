#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdatePlayerLocation.generated.h"

/**
 * BT Service: Continuously updates player location in blackboard
 */
UCLASS()
class PROJECT_CANVAS_API UBTService_UpdatePlayerLocation : public UBTService
{
	GENERATED_BODY()

public:
	UBTService_UpdatePlayerLocation();

protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector PlayerActorKey;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector DistanceToPlayerKey;
};
