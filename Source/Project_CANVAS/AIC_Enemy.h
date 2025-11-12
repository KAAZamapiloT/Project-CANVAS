// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "AIC_Enemy.generated.h"

class UAISenseConfig;
class UAIPerceptionComponent;
class UBehaviorTreeComponent;

//ENUMRATION FOR STATES IMPLEMETATION
UENUM(BlueprintType)
enum class EAICPatrolStates : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Patrolling  UMETA(DisplayName = "Patrolling"),
	Alert       UMETA(DisplayName = "Alert"),
	Chasing     UMETA(DisplayName = "Chasing"),
	Attacking   UMETA(DisplayName = "Attacking")
};






UCLASS()
class PROJECT_CANVAS_API AAIC_Enemy : public AAIController
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AAIC_Enemy();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
private:
	UPROPERTY(VisibleAnywhere,BlueprintReadWrite,Category="AI",meta=(AllowPrivateAccess=true))
	TObjectPtr<UBlackboardComponent> BlackboardComponent;
	UPROPERTY(EditInstanceOnly,BlueprintReadOnly,Category="AI",meta=(AllowPrivateAccess=true))
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="AI",meta=(AllowPrivateAccess=true))
	TObjectPtr<UBehaviorTree> BehaviorTree;

public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="AI_Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="AI_Perception")
	class UAISenseConfig_Sight* SightConfig;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="AI_Perception")
	class UAISenseConfig_Hearing* HearingConfig;
	UFUNCTION(BlueprintCallable,Category="AI_Perception")
	void SetupPerceptionSystem();
	UFUNCTION(BlueprintCallable,Blueprintable,Category="AI_Perception")
	void OnTargetDetected(AActor* InTarget,FAIStimulus Stimulus);
};
