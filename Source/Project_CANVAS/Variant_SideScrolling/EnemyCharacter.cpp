// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "HealthComponent.h"
#include "CombatAnimationComponent.h"
#include "CombatStateComponent.h"
#include "CombatDecisionEngine.h"
#include "CombatData.h"
#include "Damagable.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.016f;

    GetCharacterMovement()->MaxWalkSpeed = 600.0f;
    GetCapsuleComponent()->SetCapsuleSize(35.0f, 88.0f);
    Tags.Add(FName("Enemy.Character"));
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // ✅ Get player reference (for enemy, this is the opponent)
    PlayerCharacter = Cast<ACharacter>(GetWorld()->GetFirstPlayerController()->GetPawn());
    
    if (!PlayerCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("Enemy: Could not find player!"));
        return;
    }

    InitializeCombatComponents();
    SetupAnimations();
    BindCombatDelegates();

    if (HealthComponent)
    {
        HealthComponent->Health = MaxHealth;
        UE_LOG(LogTemp, Log, TEXT("🎮 Enemy initialized: %.0f HP"), MaxHealth);
    }

    StartCombatBehavior();
}

void AEnemyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!PlayerCharacter || !PlayerCharacter->IsValidLowLevel())
    {
        StopCombatBehavior();
        return;
    }

    if (!HealthComponent || !HealthComponent->IsAlive())
    {
        StopCombatBehavior();
        return;
    }
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopCombatBehavior();
    Super::EndPlay(EndPlayReason);
}

void AEnemyCharacter::InitializeCombatComponents()
{
    // ✅ Health Component
    HealthComponent = NewObject<UHealthComponent>(this);
    if (HealthComponent)
    {
        HealthComponent->RegisterComponent();
    }

    // ✅ Combat Animation Component
    CombatAnimationComponent = NewObject<UCombatAnimationComponent>(this);
    if (CombatAnimationComponent)
    {
        CombatAnimationComponent->RegisterComponent();
    }

    // ✅ Combat State Component (builds context)
    CombatStateComponent = NewObject<UCombatStateComponent>(this);
    if (CombatStateComponent)
    {
        CombatStateComponent->RegisterComponent();
        
        // ✅ CORRECT API: SetEnemy() instead of SetEnemyReference()
        // For EnemyCharacter, the "enemy" is the player (the opponent)
        if (PlayerCharacter)
        {
            CombatStateComponent->SetEnemy(PlayerCharacter);
            UE_LOG(LogTemp, Log, TEXT("✅ Enemy's opponent set to: %s"), *PlayerCharacter->GetName());
        }
    }

    // ✅ Combat Decision Engine (analyzes context → decides move)
    CombatDecisionEngine = NewObject<UCombatDecisionEngine>(this);
    if (CombatDecisionEngine)
    {
        CombatDecisionEngine->MoveDataTable = EnemyMoveDataTable;
        UE_LOG(LogTemp, Log, TEXT("✅ Enemy CombatDecisionEngine initialized"));
    }
}

void AEnemyCharacter::SetupAnimations()
{
    if (GetMesh() && GetMesh()->GetAnimInstance())
    {
        UE_LOG(LogTemp, Log, TEXT("✅ Enemy animations ready"));
    }
}

void AEnemyCharacter::BindCombatDelegates()
{
    if (GetMesh() && GetMesh()->GetAnimInstance())
    {
        GetMesh()->GetAnimInstance()->OnMontageEnded.AddDynamic(this, &AEnemyCharacter::OnMontageCompleted);
    }

    if (HealthComponent)
    {
        HealthComponent->OnHealthDepleted.AddDynamic(this, &AEnemyCharacter::StopCombatBehavior);
    }
}

void AEnemyCharacter::StartCombatBehavior()
{
    if (!GetWorld() || bIsInCombat)
        return;

    bIsInCombat = true;

    GetWorldTimerManager().SetTimer(
        DecisionTimerHandle,
        this,
        &AEnemyCharacter::MakeCombatDecision,
        DecisionInterval,
        true
    );

    UE_LOG(LogTemp, Log, TEXT("🎮 Enemy started combat behavior"));
}

void AEnemyCharacter::StopCombatBehavior()
{
    if (!GetWorld())
        return;

    bIsInCombat = false;
    GetWorldTimerManager().ClearTimer(DecisionTimerHandle);

    UE_LOG(LogTemp, Log, TEXT("⛔ Enemy stopped combat behavior"));
}

void AEnemyCharacter::MakeCombatDecision()
{
    // ✅ SAFETY CHECKS
    if (!HealthComponent || !HealthComponent->IsAlive())
    {
        StopCombatBehavior();
        return;
    }

    if (HealthComponent->IsStunned())
    {
        return;
    }

    if (!CanAttackPlayer())
    {
        return;
    }

    if (!CombatStateComponent || !CombatDecisionEngine)
        return;

    // ✅ BUILD CONTEXT (CombatStateComponent does this internally)
    FContextVector Context = CombatStateComponent->BuildContext(FName("EnemyAttack"));

    // ✅ LET DECISION ENGINE PICK BEST MOVE (BLACK BOX - it ranks all moves internally)
    FActionCommand Decision = CombatDecisionEngine->DecideNextMove(Context);

    // ✅ EXECUTE THE CHOSEN MOVE
    if (!Decision.MoveIdentifier.IsNone())
    {
        ExecuteMove(Decision.MoveIdentifier);
        UE_LOG(LogTemp, Log, TEXT("🗡️ Enemy executing: %s"), *Decision.MoveIdentifier.ToString());
    }
}

void AEnemyCharacter::ExecuteMove(FName MoveName)
{
    // ✅ VALIDATION
    if (!HealthComponent || !HealthComponent->IsAlive())
        return;

    if (HealthComponent->IsStunned())
        return;

    if (bIsExecutingMove)
        return;

    if (!CombatAnimationComponent || !CombatAnimationComponent->IsValidForExecution())
        return;

    // ✅ GET MOVE DATA FROM DATATABLE
    FActionCommand* MoveData = GetMoveFromDataTable(MoveName);
    if (!MoveData)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Move not found: %s"), *MoveName.ToString());
        return;
    }

    bIsExecutingMove = true;

    // ✅ EXECUTE ANIMATION
    CombatAnimationComponent->ExecuteActionCommand(*MoveData);

    UE_LOG(LogTemp, Log, TEXT("▶️ Executing: %s (Damage: %.1f)"),
        *MoveName.ToString(), MoveData->DamageToApply);
}

void AEnemyCharacter::OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted)
{
    bIsExecutingMove = false;
    UE_LOG(LogTemp, Log, TEXT("✅ Move completed"));
}

EEnemyState AEnemyCharacter::GetEnemyState() const
{
    if (!HealthComponent)
        return EEnemyState::Idle;

    if (!HealthComponent->IsAlive())
        return EEnemyState::Dead;

    if (HealthComponent->IsStunned())
        return EEnemyState::Stunned;

    if (bIsExecutingMove)
        return EEnemyState::Attacking;

    return EEnemyState::Idle;
}

float AEnemyCharacter::GetDistanceToPlayer() const
{
    if (!PlayerCharacter)
        return 999999.0f;

    return FVector::Dist(GetActorLocation(), PlayerCharacter->GetActorLocation());
}

float AEnemyCharacter::GetPlayerDirection() const
{
    if (!PlayerCharacter)
        return 0.0f;

    float PlayerX = PlayerCharacter->GetActorLocation().X;
    float EnemyX = GetActorLocation().X;

    return (PlayerX > EnemyX) ? 1.0f : -1.0f;
}

bool AEnemyCharacter::CanAttackPlayer() const
{
    if (GetDistanceToPlayer() > 500.0f)
        return false;

    if (bIsExecutingMove)
        return false;

    if (!HealthComponent || !HealthComponent->IsAlive() || HealthComponent->IsStunned())
        return false;

    return true;
}

FActionCommand* AEnemyCharacter::GetMoveFromDataTable(FName MoveName)
{
    if (!EnemyMoveDataTable)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ EnemyMoveDataTable is not set!"));
        return nullptr;
    }

    FActionCommand* MoveData = EnemyMoveDataTable->FindRow<FActionCommand>(MoveName, TEXT("GetMoveFromDataTable"));

    if (!MoveData)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Move '%s' not found in EnemyMoveDataTable"), *MoveName.ToString());
        return nullptr;
    }

    return MoveData;
}

// ========== IDAMAGABLE INTERFACE IMPLEMENTATION ==========

void AEnemyCharacter::ReceiveDamage_Implementation(const FDamageSpec& Spec)
{
    if (!HealthComponent || !HealthComponent->IsAlive())
    {
        return;
    }

    HealthComponent->ApplyDamage(Spec.Amount, EDamageType::Physical, Spec.HitLocation);

    UE_LOG(LogTemp, Warning, TEXT("💥 Enemy took %.1f damage"), Spec.Amount);

    float StunDuration = 0.3f;
    HealthComponent->ApplyStun(StunDuration);
}

bool AEnemyCharacter::IsAlive_Implementation() const
{
    if (!HealthComponent)
        return false;

    return HealthComponent->IsAlive();
}
