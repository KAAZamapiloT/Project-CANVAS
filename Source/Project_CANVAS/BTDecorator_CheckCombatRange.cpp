#include "BTDecorator_CheckCombatRange.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTDecorator_CheckCombatRange::UBTDecorator_CheckCombatRange()
{
	NodeName = "Check Combat Range";
}

bool UBTDecorator_CheckCombatRange::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
	if (!Blackboard)
		return false;

	float Distance = Blackboard->GetValueAsFloat(DistanceKey.SelectedKeyName);

	bool bInRange = false;

	switch (RequiredRange)
	{
	case EAICombatRange::ECR_Melle:
		bInRange = (Distance >= 0.0f && Distance <= 200.0f);
		break;
	case EAICombatRange::ECR_MID:
		bInRange = (Distance > 200.0f && Distance <= 500.0f);
		break;
	case EAICombatRange::ECR_FAR:
		bInRange = (Distance > 500.0f && Distance <= 1000.0f);
		break;
	case EAICombatRange::ECR_VFAR:
		bInRange = (Distance > 1000.0f);
		break;
		default:
		Distance=9999.9f;
		
	}

	return bInRange;
}
