// Fill out your copyright notice in the Description page of Project Settings.


// EnemyCharacter.cpp
// 2.5D enemy implementation with plane-constrained movement

#include "EnemyCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HealthComponent.h"
#include "CombatAnimationComponent.h"
#include "Damagable.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AEnemyCharacter::AEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Add "Enemy" tag for player's FindNearestEnemy() function
    Tags.Add(FName("Enemy"));

    // =====================================
    // 2.5D PLANE CONSTRAINT (KEY FIX)
    // =====================================
    
    // Lock movement to X-Z plane (side-scroller zone)
    // Prevents enemy from drifting in Y-axis during navigation
    GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 1.0f, 0.0f));
    GetCharacterMovement()->bConstrainToPlane = true;
    
    // Make enemy face movement direction (left/right in 2.5D)
    GetCharacterMovement()->bOrientRotationToMovement = true;
    
    // Disable controller yaw rotation (side-scroller doesn't need it)
    bUseControllerRotationYaw = false;

    UE_LOG(LogTemp, Log, TEXT("Enemy CharacterMovement constrained to 2.5D plane"));

    // =====================================
    // COMBAT COMPONENTS
    // =====================================
    
    // Create health component
    HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));
    
    // Create combat animation component
    CombatAnimComp = CreateDefaultSubobject<UCombatAnimationComponent>(TEXT("CombatAnimComp"));
}

void AEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // =====================================
    // WIRE HEALTH DELEGATES
    // =====================================
    
    if (HealthComp)
    {
        HealthComp->OnHealthChanged.AddDynamic(this, &AEnemyCharacter::OnHealthChanged);
        HealthComp->OnHealthDepleted.AddDynamic(this, &AEnemyCharacter::OnDeath);
        
        UE_LOG(LogTemp, Log, TEXT("Enemy health initialized: %.1f HP"), HealthComp->MaxHealth);
    }

    // =====================================
    // WIRE COMBAT ANIMATION DELEGATES
    // =====================================
    
    if (CombatAnimComp)
    {
        CombatAnimComp->OnMontageEnded.AddDynamic(this, &AEnemyCharacter::OnMoveCompleted);
        CombatAnimComp->OnHitWindowActive.AddDynamic(this, &AEnemyCharacter::OnHitWindowActive);
        
        UE_LOG(LogTemp, Log, TEXT("Enemy combat animation component initialized"));
    }

    // =====================================
    // VALIDATE PLANE CONSTRAINT
    // =====================================
    
    // Log current Y position to verify constraint is working
    FVector CurrentLocation = GetActorLocation();
    UE_LOG(LogTemp, Log, TEXT("Enemy spawned at Y=%.1f (should stay constant)"), CurrentLocation.Y);
}

void AEnemyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // =====================================
    // DEBUG: MONITOR Y-AXIS DRIFT
    // =====================================
    // Uncomment to verify plane constraint is working
    /*
    FVector CurrentLocation = GetActorLocation();
    if (FMath::Abs(CurrentLocation.Y - SpawnY) > 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Enemy drifting in Y! Current: %.1f, Spawn: %.1f"), 
               CurrentLocation.Y, SpawnY);
    }
    */
}

// =====================================
// IDAMAGABLE INTERFACE IMPLEMENTATION
// =====================================

void AEnemyCharacter::ReceiveDamage_Implementation(const FDamageSpec& Spec)
{
    // Route damage to HealthComponent
    // This replaces your old IIA_Damageable::Damage(100) function
    if (HealthComp && HealthComp->IsAlive())
    {
        HealthComp->ApplyDamage(Spec.Amount);
        
        // TODO: Play hit react montage
        // TODO: Apply stun via gameplay tags
        
        UE_LOG(LogTemp, Warning, TEXT("Enemy received %.1f damage from %s"), 
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
    UE_LOG(LogTemp, Log, TEXT("Enemy move completed: %s"), *CompletedMove.ToString());
    
    // AI controller will query Decision Engine for next move here
    // (Implemented in AI controller, not character class)
}

void AEnemyCharacter::OnHitWindowActive(float Damage, float Stun)
{
    UE_LOG(LogTemp, Log, TEXT("Enemy hit window active: Damage=%.1f, Stun=%.1f"), Damage, Stun);
    
    // Perform hit detection when AnimNotify fires
    PerformHitDetection(Damage, Stun);
}

void AEnemyCharacter::PerformHitDetection(float Damage, float Stun)
{
    // =====================================
    // SPHERE TRACE FOR HIT DETECTION
    // Adapted from your AttackTrace() line trace
    // =====================================
    
    FVector Start = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    FVector End = Start + (Forward * 150.f);  // 150 unit attack range

    // Perform sphere trace
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);  // Ignore self

    bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,  // Trace against pawns
        FCollisionShape::MakeSphere(50.f),  // 50 unit radius
        QueryParams
    );

    // Draw debug visualization (green if hit, red if miss)
    DrawDebugSphere(GetWorld(), End, 50.f, 12, bHit ? FColor::Green : FColor::Red, false, 0.5f);

    // =====================================
    // APPLY DAMAGE VIA INTERFACE
    // =====================================
    
    if (bHit && HitResult.GetActor())
    {
        AActor* Target = HitResult.GetActor();
        
        UE_LOG(LogTemp, Log, TEXT("Enemy hit target: %s"), *Target->GetName());

        // Check if target implements IDamagable
        if (Target->GetClass()->ImplementsInterface(UDamagable::StaticClass()))
        {
            // Build damage spec
            FDamageSpec Spec;
            Spec.Amount = Damage;
            Spec.HitLocation = HitResult.ImpactPoint;
            Spec.HitNormal = HitResult.ImpactNormal;
            Spec.HitBone = HitResult.BoneName;
            Spec.InstigatorController = GetController();
            Spec.DamageCauser = this;

            // Apply damage (replaces your old Damageable->Damage(100))
            IDamagable::Execute_ReceiveDamage(Target, Spec);
            
            UE_LOG(LogTemp, Log, TEXT("Enemy dealt %.1f damage to %s"), Damage, *Target->GetName());
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Target does not implement IDamagable"));
        }
    }
}

// =====================================
// HEALTH DELEGATES
// =====================================

void AEnemyCharacter::OnHealthChanged(float Current, float Max)
{
    UE_LOG(LogTemp, Log, TEXT("Enemy Health: %.1f / %.1f"), Current, Max);
    
    // TODO: Update health bar widget
}

void AEnemyCharacter::OnDeath()
{
    UE_LOG(LogTemp, Warning, TEXT("Enemy died!"));
    
    // Stop AI behavior
    if (UBehaviorTree* BT = BehaviorTree)
    {
        // AI controller will handle stopping behavior tree
    }
    
    // TODO: Play death animation
    // TODO: Disable collision
    // TODO: Destroy after delay
    
    SetLifeSpan(3.0f);  // Destroy after 3 seconds
}

