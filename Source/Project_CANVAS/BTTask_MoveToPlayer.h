#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MoveToPlayer.generated.h"

UCLASS()
class PROJECT_CANVAS_API UBTTask_MoveToPlayer : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MoveToPlayer();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	UPROPERTY(EditAnywhere, Category="Movement")
	float AcceptanceRadius = 150.0f;

	UPROPERTY(EditAnywhere, Category="Movement")
	float MoveSpeed = 400.0f;// ✅ Helper to flip enemy sprite
	void FacePlayer(class AEnemyCharacter* Enemy, class ACharacter* Player);
};
