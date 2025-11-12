// Fill out your copyright notice in the Description page of Project Settings.

#include "AIC_Enemy.h"
#include "SideScrollingCharacter.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"  // ADD THIS
#include "Perception/AISenseConfig_Sight.h"

AAIC_Enemy::AAIC_Enemy()
{
    PrimaryActorTick.bCanEverTick = true;
    BlackboardComponent = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackBoard Component"));
    BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("Behavior Tree Component"));
    SetupPerceptionSystem();
}

void AAIC_Enemy::BeginPlay()
{
    Super::BeginPlay();
    if (GetPerceptionComponent())
    {
        GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AAIC_Enemy::OnTargetDetected);
        UE_LOG(LogTemp, Log, TEXT("✅ Perception delegates bound"));
    }
    // ✅ Don't run BT here - wait for OnPossess
    if (BehaviorTree)
    {
        UE_LOG(LogTemp, Log, TEXT("✅ BehaviorTree assigned: %s"), *BehaviorTree->GetName());
    }
}

void AAIC_Enemy::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    
    UE_LOG(LogTemp, Warning, TEXT("🎮 AI Controller possessing: %s"), 
            InPawn ? *InPawn->GetName() : TEXT("NULL"));
    
    if (BlackboardComponent && BehaviorTree && BehaviorTree->BlackboardAsset)
    {
        BlackboardComponent->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
        UE_LOG(LogTemp, Warning, TEXT("✅ Blackboard initialized"));
        
        // ✅ UNCOMMENT THIS LINE:
        RunBehaviorTree(BehaviorTree);
        UE_LOG(LogTemp, Warning, TEXT("✅ BehaviorTree started: %s"), *BehaviorTree->GetName());
    }
    else
    {
        if (!BehaviorTree)
            UE_LOG(LogTemp, Error, TEXT("❌ No BehaviorTree asset assigned!"));
        if (!BlackboardComponent)
            UE_LOG(LogTemp, Error, TEXT("❌ BlackboardComponent is NULL!"));
    }
}

void AAIC_Enemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AAIC_Enemy::SetupPerceptionSystem()
{
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing Config"));
    SetPerceptionComponent(*CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception Component")));

    if (HearingConfig)
    {
        HearingConfig->HearingRange = 3000.0f;
        HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
        HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
        HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
        HearingConfig->SetMaxAge(20.f);
        HearingConfig->SetStartsEnabled(true);
        GetPerceptionComponent()->ConfigureSense(*HearingConfig);
    }

    if (SightConfig)
    {
        SightConfig->SightRadius = 800.0f;
        SightConfig->LoseSightRadius = SightConfig->SightRadius + 25.f;
        SightConfig->PeripheralVisionAngleDegrees = 75.0f;
        SightConfig->SetMaxAge(5.f);
        SightConfig->AutoSuccessRangeFromLastSeenLocation = 550.f;
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        GetPerceptionComponent()->SetDominantSense(*SightConfig->GetSenseImplementation());
        
        // FIX: Remove duplicate AAIC_Enemy::
       // GetPerceptionComponent()->OnTargetPerceptionUpdated.AddDynamic(this, &AAIC_Enemy::OnTargetDetected);
        
        GetPerceptionComponent()->ConfigureSense(*SightConfig);
    }
}

void AAIC_Enemy::OnTargetDetected(AActor* InTarget, FAIStimulus Stimulus)
{
    if (auto* const ch = Cast<ASideScrollingCharacter>(InTarget))
    {
        BlackboardComponent->SetValueAsBool("IsSeeingPlayer", Stimulus.WasSuccessfullySensed());
    }
    else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
    {
        UE_LOG(LogTemp, Warning, TEXT("HEARING Loop was entered"));
        if (Stimulus.WasSuccessfullySensed())
        {
            BlackboardComponent->SetValueAsBool("IsHearingPlayer", true);
            BlackboardComponent->SetValueAsVector("LastHeardLocation", Stimulus.StimulusLocation);
            UE_LOG(LogTemp, Warning, TEXT("HEARING WAS SENSED"));
        }
    }
}
