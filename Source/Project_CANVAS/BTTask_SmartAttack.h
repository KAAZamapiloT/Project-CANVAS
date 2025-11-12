// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_SmartAttack.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_CANVAS_API UBTTask_SmartAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_SmartAttack();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds);
protected:
	/** Use decision engine for smart move selection */
	UPROPERTY(EditAnywhere, Category="Combat")
	bool bUseDecisionEngine = true;

	/** Fallback move if decision engine fails */
	UPROPERTY(EditAnywhere, Category="Combat")
	FName FallbackMove = FName("LightAttack");
};
