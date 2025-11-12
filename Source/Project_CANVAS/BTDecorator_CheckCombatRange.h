#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTDecorator.h"
#include "CombatData.h"
#include "BTDecorator_CheckCombatRange.generated.h"

/**
 * Checks if enemy is in specific combat range
 */


UCLASS()
class PROJECT_CANVAS_API UBTDecorator_CheckCombatRange : public UBTDecorator
{
	GENERATED_BODY()

public:
	UBTDecorator_CheckCombatRange();

protected:
	virtual bool CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const override;

	UPROPERTY(EditAnywhere, Category="Combat")
	EAICombatRange RequiredRange = EAICombatRange::ECR_Melle;

	UPROPERTY(EditAnywhere, Category="Blackboard")
	FBlackboardKeySelector DistanceKey;
};
