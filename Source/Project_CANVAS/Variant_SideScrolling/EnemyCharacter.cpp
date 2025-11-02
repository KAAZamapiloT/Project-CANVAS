// Fill out your copyright notice in the Description page of Project Settings.


// EnemyCharacter.cpp
// Enemy executes attacks directly via ExecuteAttack() from behavior tree

#include "EnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HealthComponent.h"
#include "CombatAnimationComponent.h"
#include "CombatData.h"  // For FActionCommand
#include "CombatDecisionEngine.h"
#include "CombatStateComponent.h"
#include "Damagable.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = false;
    Tags.Add(FName("Enemy"));

    // =====================================
    // 2.5D PLANE CONSTRAINT
    // =====================================
    
    GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 1.0f, 0.0f));
    GetCharacterMovement()->bConstrainToPlane = true;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    bUseControllerRotationYaw = false;

    // =====================================
    // COMBAT COMPONENTS
    // =====================================
    
    HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));
    CombatAnimComp = CreateDefaultSubobject<UCombatAnimationComponent>(TEXT("CombatAnimComp"));
    CombatStateComp = CreateDefaultSubobject<UCombatStateComponent>(TEXT("CombatStateComp"));
    Tags.Add("Enemy");
   
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // ✅ Create UObject DecisionEngine in BeginPlay
    CombatDecisionComp = NewObject<UCombatDecisionEngine>(this, UCombatDecisionEngine::StaticClass());
    
    // Wire health delegates
    if (HealthComp)
    {
        HealthComp->OnHealthChanged.AddDynamic(this, &AEnemyCharacter::OnHealthChanged);
        HealthComp->OnHealthDepleted.AddDynamic(this, &AEnemyCharacter::OnDeath);
    }

    // Wire combat animation delegates
    if (CombatAnimComp)
    {
        CombatAnimComp->OnMontageEnded.AddDynamic(this, &AEnemyCharacter::OnMoveCompleted);
        CombatAnimComp->OnHitWindowActive.AddDynamic(this, &AEnemyCharacter::OnHitWindowActive);
    }
    if (CombatStateComp)
    {
        ACharacter* PlayerChar = GetWorld()->GetFirstPlayerController()->GetCharacter();
        CombatStateComp->SetEnemy(PlayerChar);
    }

    // ADD engine initialization
    if (CombatDecisionComp)
    {
        CombatDecisionComp->MoveDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Varient_SideScrolling/Blueprints/DT_CombatMoves"));
    }
}

// =====================================
// BEHAVIOR TREE CALLABLE ATTACK
// Called from BTTask_EnemyAttack
// =====================================

void AEnemyCharacter::ExecuteAttack()
{
  

    // Check if already attacking
    if (CombatAnimComp->IsExecutingMove())
    {
        UE_LOG(LogTemp, Warning, TEXT("Enemy already executing attack"));
        return;
    }
    FContextVector Context=CombatStateComp->BuildContext(FName("AI_Attack"));
    // Build Action Command manually (no Decision Engine)
    FActionCommand Command;
    Command = CombatDecisionComp->DecideNextMove(Context);
   
    // Execute via CombatAnimationComponent
    CombatAnimComp->ExecuteActionCommand(Command);

    UE_LOG(LogTemp, Log, TEXT("Enemy executing %s (Damage=%.1f, Stun=%.1f)"), 
    *Command.MoveIdentifier.ToString(), 
    Command.DamageToApply, 
    Command.StunDurationToInflict);

}

// =====================================
// IDAMAGABLE INTERFACE
// =====================================

void AEnemyCharacter::ReceiveDamage_Implementation(const FDamageSpec& Spec)
{
    if (HealthComp && HealthComp->IsAlive())
    {
        HealthComp->ApplyDamage(Spec.Amount);
        
        UE_LOG(LogTemp, Warning, TEXT("Enemy took %.1f damage from %s"), 
               Spec.Amount, 
               Spec.DamageCauser ? *Spec.DamageCauser->GetName() : TEXT("Unknown"));
    }
}

bool AEnemyCharacter::IsAlive_Implementation() const
{
    return HealthComp && HealthComp->IsAlive();
}

// =====================================
// COMBAT ANIMATION DELEGATES
// =====================================

void AEnemyCharacter::OnMoveCompleted(FName CompletedMove)
{
    UE_LOG(LogTemp, Log, TEXT("Enemy attack completed: %s"), *CompletedMove.ToString());
    // Behavior tree will handle next action (return to patrol, chase, etc.)
}

void AEnemyCharacter::OnHitWindowActive(float Damage, float Stun)
{
    // AnimNotify fired - perform hit detection
    PerformHitDetection(Damage, Stun);
}

void AEnemyCharacter::PerformHitDetection(float Damage, float Stun)
{
    // Sphere trace for hit detection
    FVector Start = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    FVector End = Start + (Forward * 150.f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(50.f),
        QueryParams
    );

    DrawDebugSphere(GetWorld(), End, 50.f, 12, bHit ? FColor::Green : FColor::Red, false, 0.5f);

    if (bHit && HitResult.GetActor())
    {
        AActor* Target = HitResult.GetActor();

        if (Target->GetClass()->ImplementsInterface(UDamagable::StaticClass()))
        {
            FDamageSpec Spec;
            Spec.Amount = Damage;
            Spec.HitLocation = HitResult.ImpactPoint;
            Spec.HitNormal = HitResult.ImpactNormal;
            Spec.HitBone = HitResult.BoneName;
            Spec.InstigatorController = GetController();
            Spec.DamageCauser = this;

            IDamagable::Execute_ReceiveDamage(Target, Spec);
            
            UE_LOG(LogTemp, Log, TEXT("Enemy hit %s for %.1f damage"), *Target->GetName(), Damage);
        }
    }
}

// =====================================
// HEALTH DELEGATES
// =====================================

void AEnemyCharacter::OnHealthChanged(float Current, float Max)
{
    UE_LOG(LogTemp, Log, TEXT("Enemy Health: %.1f / %.1f"), Current, Max);
}

void AEnemyCharacter::OnDeath()
{
    UE_LOG(LogTemp, Warning, TEXT("Enemy died!"));
    SetLifeSpan(3.0f);
}
