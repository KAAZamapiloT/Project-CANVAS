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
#include "AIController.h"
#include "AIC_Enemy.h"
#include"GameFramework/GameModeBase.h"
#include "DrawDebugHelpers.h" 
//==============================================================================================
// CONSTRUCTOR
//==============================================================================================
AEnemyCharacter::AEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.016f;

	// Sidescroller movement
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->bConstrainToPlane = true;
	GetCharacterMovement()->SetPlaneConstraintAxisSetting(EPlaneConstraintAxisSetting::Y);
	GetCapsuleComponent()->SetCapsuleSize(35.0f, 88.0f);
	Tags.Add(FName("Enemy.Character"));
	AIControllerClass = AAIC_Enemy::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// ✅ ADD THESE LINES FOR AUTO-FACING:
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
    
	// ✅ ADD THESE LINES FOR DOUBLE JUMP:
	GetCharacterMovement()->GravityScale = 1.75f;
	GetCharacterMovement()->JumpZVelocity = 750.0f;
	GetCharacterMovement()->AirControl = 1.0f;
	JumpMaxCount = 4; // 1 ground jump + 3 air jumps
	
	UE_LOG(LogTemp, Display, TEXT("🏗️ [ENEMY] Constructor called"));
}

//==============================================================================================
// LIFECYCLE
//==============================================================================================
void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Warning, TEXT("🤖 [ENEMY %s] BeginPlay started"), *GetName());
	
	InitializeCombatComponents();
	BindCombatDelegates();
	
	if (HealthComponent)
	{
		HealthComponent->MaxHealth = MaxHealth;
		HealthComponent->Health = MaxHealth;
		UE_LOG(LogTemp, Display, TEXT("✅ [ENEMY %s] Initialized: %.0f HP"), *GetName(), MaxHealth);
	}

	// Enemy Character
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	if (Player && CombatStateComponent)
	{
		CombatStateComponent->SetEnemy(Player);
		UE_LOG(LogTemp, Display, TEXT("✅ [ENEMY %s] Target set: %s"), *GetName(), *Player->GetName());
	}

	PlayerCharacter = Cast<ACharacter>(GetWorld()->GetFirstPlayerController()->GetPawn());
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ [ENEMY %s] Player not found yet"), *GetName());
	}
	else if (CombatStateComponent)
	{
		CombatStateComponent->SetEnemy(PlayerCharacter);
		UE_LOG(LogTemp, Display, TEXT("✅ [ENEMY %s] Opponent set: %s"), *GetName(), *PlayerCharacter->GetName());
	}

	StartCombatBehavior();
	UE_LOG(LogTemp, Warning, TEXT("✅ [ENEMY %s] Combat started (Pure C++ Mode)"), *GetName());
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!PlayerCharacter || !PlayerCharacter->IsValidLowLevel())
	{
		StopCombatBehavior();
		return;
	}
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UE_LOG(LogTemp, Log, TEXT("🛑 [ENEMY %s] EndPlay"), *GetName());
	StopCombatBehavior();
	Super::EndPlay(EndPlayReason);
}

//==============================================================================================
// INITIALIZATION
//==============================================================================================
void AEnemyCharacter::InitializeCombatComponents()
{
	UE_LOG(LogTemp, Display, TEXT("⚙️ [ENEMY %s] Initializing components"), *GetName());
	
	HealthComponent = NewObject<UHealthComponent>(this);
	if (HealthComponent)
	{
		HealthComponent->RegisterComponent();
		UE_LOG(LogTemp, Display, TEXT("   ✅ HealthComponent created"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("   ❌ HealthComponent FAILED to create!"));
	}

	CombatAnimationComponent = NewObject<UCombatAnimationComponent>(this);
	if (CombatAnimationComponent)
	{
		CombatAnimationComponent->RegisterComponent();
		UE_LOG(LogTemp, Display, TEXT("   ✅ CombatAnimationComponent created"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("   ❌ CombatAnimationComponent FAILED to create!"));
	}

	CombatStateComponent = NewObject<UCombatStateComponent>(this);
	if (CombatStateComponent)
	{
		CombatStateComponent->RegisterComponent();
		UE_LOG(LogTemp, Display, TEXT("   ✅ CombatStateComponent created"));
		if (PlayerCharacter)
		{
			CombatStateComponent->SetEnemy(PlayerCharacter);
			UE_LOG(LogTemp, Display, TEXT("   ✅ Opponent set: %s"), *PlayerCharacter->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("   ❌ CombatStateComponent FAILED to create!"));
	}

	CombatDecisionEngine = NewObject<UCombatDecisionEngine>(this);
	if (CombatDecisionEngine)
	{
		CombatDecisionEngine->MoveDataTable = EnemyMoveDataTable;
		UE_LOG(LogTemp, Display, TEXT("   ✅ CombatDecisionEngine created"));
		if (EnemyMoveDataTable)
		{
			UE_LOG(LogTemp, Display, TEXT("   ✅ DataTable assigned: %s"), *EnemyMoveDataTable->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("   ❌ EnemyMoveDataTable is NULL!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("   ❌ CombatDecisionEngine FAILED to create!"));
	}
}

void AEnemyCharacter::BindCombatDelegates()
{
	UE_LOG(LogTemp, Display, TEXT("🔗 [ENEMY %s] Binding delegates"), *GetName());
	
	if (!HealthComponent || !CombatStateComponent || !CombatAnimationComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] Cannot bind - components null!"), *GetName());
		return;
	}

	HealthComponent->OnHealthDepleted.AddDynamic(this, &AEnemyCharacter::OnHealthDepletedHandler);
	HealthComponent->OnDamageTaken.AddDynamic(this, &AEnemyCharacter::OnDamageTakenHandler);
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->OnMontageEnded.AddDynamic(this, &AEnemyCharacter::OnMontageCompleted);
		UE_LOG(LogTemp, Display, TEXT("✅ [ENEMY %s] Delegates bound"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] AnimInstance NULL - montage delegate NOT bound!"), *GetName());
	}
}

//==============================================================================================
// COMBAT BEHAVIOR
//==============================================================================================
void AEnemyCharacter::StartCombatBehavior()
{
	UE_LOG(LogTemp, Warning, TEXT("🔄 [ENEMY %s] StartCombatBehavior() called"), *GetName());
	
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] Cannot start - World is NULL!"), *GetName());
		return;
	}
	
	if (bIsInCombat)
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ [ENEMY %s] Already in combat - skipping"), *GetName());
		return;
	}
	
	if (IsBehaviorTreeActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("⚠️ [ENEMY %s] BehaviorTree active - skipping timer"), *GetName());
		return;
	}

	bIsInCombat = true;
	GetWorld()->GetTimerManager().SetTimer(
		DecisionTimerHandle,
		this,
		&AEnemyCharacter::MakeCombatDecision,
		DecisionInterval,
		true
	);
	UE_LOG(LogTemp, Warning, TEXT("✅ [ENEMY %s] Timer started: %.2fs interval"), *GetName(), DecisionInterval);
}

void AEnemyCharacter::StopCombatBehavior()
{
	if (!bIsInCombat)
	{
		UE_LOG(LogTemp, Display, TEXT("⚠️ [ENEMY %s] Not in combat - nothing to stop"), *GetName());
		return;
	}

	bIsInCombat = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(DecisionTimerHandle);
	}
	UE_LOG(LogTemp, Warning, TEXT("⏸️ [ENEMY %s] Combat stopped"), *GetName());
}

void AEnemyCharacter::MakeCombatDecision()
{
	UE_LOG(LogTemp, Warning, TEXT("🎯 [ENEMY %s] MakeCombatDecision() CALLED"), *GetName());
	
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] PlayerCharacter is NULL"), *GetName());
		return;
	}
	
	if (!CombatDecisionEngine)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] CombatDecisionEngine is NULL"), *GetName());
		return;
	}
	
	if (!CombatStateComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] CombatStateComponent is NULL"), *GetName());
		return;
	}

	if (CombatAnimationComponent && CombatAnimationComponent->IsExecutingMove())
	{
		UE_LOG(LogTemp, Display, TEXT("⏸️ [ENEMY %s] Already executing move - skipping"), *GetName());
		return;
	}

	if (!HealthComponent || !HealthComponent->IsAlive() || HealthComponent->IsStunned())
	{
		UE_LOG(LogTemp, Display, TEXT("⏸️ [ENEMY %s] Dead or stunned - skipping (Alive: %d, Stunned: %d)"), 
			*GetName(), 
			HealthComponent ? HealthComponent->IsAlive() : false,
			HealthComponent ? HealthComponent->IsStunned() : false);
		return;
	}
	if (CanAttackPlayer())
	{
		// ✅ FACE PLAYER BEFORE ATTACKING
		if (PlayerCharacter)
		{
			float DeltaX = PlayerCharacter->GetActorLocation().X - GetActorLocation().X;
			FRotator NewRotation = GetActorRotation();
			NewRotation.Yaw = (DeltaX > 0) ? 0.0f : 180.0f;
			SetActorRotation(NewRotation);
            
			UE_LOG(LogTemp, Display, TEXT("👁️ [ENEMY] Facing player for attack"));
		}
		UE_LOG(LogTemp, Display, TEXT("✅ [ENEMY %s] All checks passed - executing decision"), *GetName());
	
		FContextVector Context = CombatStateComponent->BuildContext(FName("LightAttack"));
		FActionCommand Decision = CombatDecisionEngine->DecideNextMove(Context);
	
		if (!Decision.MoveIdentifier.IsNone() && CombatAnimationComponent)
		{
			UE_LOG(LogTemp, Warning, TEXT("⚡ [ENEMY %s] Executing move: %s"), *GetName(), *Decision.MoveIdentifier.ToString());
			CombatAnimationComponent->ExecuteActionCommand(Decision);
		}
		else
		{
			if (Decision.MoveIdentifier.IsNone())
			{
				UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] Decision returned None!"), *GetName());
			}
			if (!CombatAnimationComponent)
			{
				UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] CombatAnimationComponent is NULL!"), *GetName());
			}
		}
	}
}
void AEnemyCharacter::OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted)
{
	UE_LOG(LogTemp, Display, TEXT("✅ [ENEMY %s] Montage completed: %s (Interrupted: %d)"), 
		*GetName(), 
		Montage ? *Montage->GetName() : TEXT("NULL"), 
		bInterrupted);
}

void AEnemyCharacter::OnHealthDepletedHandler()
{
	StopCombatBehavior();
	UE_LOG(LogTemp, Error, TEXT("💀 [ENEMY %s] DIED!"), *GetName());
}

//==============================================================================================
// STATE QUERIES
//==============================================================================================
EEnemyState AEnemyCharacter::GetEnemyState() const
{
	if (!HealthComponent || !HealthComponent->IsAlive())
	{
		return EEnemyState::Dead;
	}
	
	if (HealthComponent->IsStunned())
	{
		return EEnemyState::Stunned;
	}
	
	if (CombatAnimationComponent && CombatAnimationComponent->IsExecutingMove())
	{
		return EEnemyState::Attacking;
	}
	
	return EEnemyState::Idle;
}

bool AEnemyCharacter::CanAttackPlayer() const
{
	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Display, TEXT("🔍 [ENEMY %s] CanAttack: No PlayerCharacter"), *GetName());
		return false;
	}

	if (CombatAnimationComponent && CombatAnimationComponent->IsExecutingMove())
	{
		UE_LOG(LogTemp, Display, TEXT("🔍 [ENEMY %s] CanAttack: Already executing move"), *GetName());
		return false;
	}

	if (!HealthComponent || !HealthComponent->IsAlive() || HealthComponent->IsStunned())
	{
		UE_LOG(LogTemp, Display, TEXT("🔍 [ENEMY %s] CanAttack: Dead or Stunned"), *GetName());
		return false;
	}

	if (CombatStateComponent)
	{
		float Distance = CombatStateComponent->GetDistanceToEnemy();
		bool bInRange = (Distance > 0.0f && Distance <= MinAttackDistance);
		UE_LOG(LogTemp, Display, TEXT("🔍 [ENEMY %s] CanAttack: Distance=%.1f, MinDist=%.1f, InRange=%d"), 
			*GetName(), Distance, MinAttackDistance, bInRange);
		return bInRange;
	}

	UE_LOG(LogTemp, Display, TEXT("🔍 [ENEMY %s] CanAttack: No CombatStateComponent"), *GetName());
	return false;
}

//==============================================================================================
// AI HELPERS
//==============================================================================================
bool AEnemyCharacter::IsBehaviorTreeActive() const
{
	if (AAIController* AIC = Cast<AAIController>(GetController()))
	{
		UBrainComponent* Brain = AIC->GetBrainComponent();
		// ✅ Check if Brain exists AND is actively running
		bool bActive = Brain != nullptr && Brain->IsRunning();
		UE_LOG(LogTemp, Display, TEXT("🧠 [ENEMY %s] BehaviorTree active: %d (Brain: %d, Running: %d)"), 
			*GetName(), bActive, Brain != nullptr, Brain ? Brain->IsRunning() : false);
		return bActive;
	}
	return false;
}


AAIController* AEnemyCharacter::GetAIController() const
{
	return Cast<AAIController>(GetController());
}

//==============================================================================================
// IDAMAGABLE INTERFACE
//==============================================================================================
void AEnemyCharacter::ReceiveDamage_Implementation(const FDamageSpec& Spec)
{
	if (!HealthComponent || !HealthComponent->IsAlive())
		return;
if (bShowDebug)
{
	// ✅ DRAW DEBUG SPHERE - Enemy taking damage
	DrawDebugSphere(
		GetWorld(),
		GetActorLocation(),
		90.0f,
		12,
		FColor::Blue,  // Blue = enemy damaged
		false,
		2.0f,
		0,
		4.0f
	);
}
	

	// ✅ SIMPLE: Forward to HealthComponent
	HealthComponent->ApplyDamage(Spec.Amount, EDamageType::Electric, Spec.HitLocation);

	UE_LOG(LogTemp, Error, TEXT("💥 ENEMY took %.1f damage! HP: %.1f/%.1f"),
		   Spec.Amount,
		   HealthComponent->Health,
		   HealthComponent->MaxHealth);
}

bool AEnemyCharacter::IsAlive_Implementation() const
{
	return HealthComponent && HealthComponent->IsAlive();
}

//==============================================================================================
// EXECUTE MOVE (BT CALLABLE)
//==============================================================================================
void AEnemyCharacter::ExecuteMove(FName MoveName)
{
    UE_LOG(LogTemp, Warning, TEXT("══════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("🎬 [ENEMY %s] EXECUTE MOVE: %s"), *GetName(), *MoveName.ToString());
    UE_LOG(LogTemp, Warning, TEXT("══════════════════════════════════════════════════"));

    // ═════════════════════════════════════════════════════════
    // VALIDATION
    // ═════════════════════════════════════════════════════════
    if (!CombatStateComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] CombatStateComponent is NULL!"), *GetName());
        return;
    }

    if (!CombatDecisionEngine)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] CombatDecisionEngine is NULL!"), *GetName());
        return;
    }

    if (!CombatAnimationComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY %s] CombatAnimationComponent is NULL!"), *GetName());
        return;
    }

    if (MoveName.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ [ENEMY %s] MoveName is None!"), *GetName());
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("✅ [ENEMY %s] All components valid"), *GetName());

    // ═════════════════════════════════════════════════════════
    // FORCE FACE PLAYER BEFORE ATTACKING
    // ═════════════════════════════════════════════════════════
    if (PlayerCharacter)
    {
        float DeltaX = PlayerCharacter->GetActorLocation().X - GetActorLocation().X;
        FRotator NewRotation = GetActorRotation();
        NewRotation.Yaw = (DeltaX > 0) ? 0.0f : 180.0f;  // Face right (0°) or left (180°)
        SetActorRotation(NewRotation);
        
        UE_LOG(LogTemp, Error, TEXT("👁️ [ENEMY] Facing player (Yaw: %.0f) DeltaX: %.1f"), NewRotation.Yaw, DeltaX);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY] No PlayerCharacter reference - can't face target!"));
    }

    // ═════════════════════════════════════════════════════════
    // BUILD CONTEXT
    // ═════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Display, TEXT("⚙️ [ENEMY %s] Building context..."), *GetName());
    FContextVector Context = CombatStateComponent->BuildContext(MoveName);

    // ═════════════════════════════════════════════════════════
    // DECIDE MOVE
    // ═════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Display, TEXT("🧠 [ENEMY %s] Deciding move..."), *GetName());
    FActionCommand Command = CombatDecisionEngine->DecideNextMove(Context);
    
    UE_LOG(LogTemp, Warning, TEXT("📋 [ENEMY %s] Action: %s | Damage: %.1f | Stun: %.1f"), 
           *GetName(), 
           *Command.MoveIdentifier.ToString(),
           Command.DamageToApply,           // ✅ CORRECT FIELD NAME
           Command.StunDurationToInflict);  // ✅ CORRECT FIELD NAME

    // ═════════════════════════════════════════════════════════
    // SCHEDULE DAMAGE TEST (Temporary - replaces AnimNotify)
    // ═════════════════════════════════════════════════════════
    UE_LOG(LogTemp, Display, TEXT("⏲️ [ENEMY] Scheduling damage test in 0.4 seconds..."));
    
    GetWorld()->GetTimerManager().SetTimer(
        TempDamageTestHandle,
        FTimerDelegate::CreateLambda([this, Command]() {
            UE_LOG(LogTemp, Warning, TEXT("⚡ [ENEMY] DAMAGE TEST TRIGGERED!"));
            TestDirectDamage(Command.DamageToApply);  // ✅ CORRECT FIELD NAME
        }),
        0.4f,  // Damage after 0.4 seconds (mid-animation)
        false
    );

    // ═════════════════════════════════════════════════════════
    // EXECUTE ANIMATION
    // ═════════════════════════════════════════════════════════
    CombatAnimationComponent->ExecuteActionCommand(Command);

    // ═════════════════════════════════════════════════════════
    // SET COOLDOWN
    // ═════════════════════════════════════════════════════════
    CombatStateComponent->StartCooldown(Command.MoveIdentifier, 0.5f);
    
    UE_LOG(LogTemp, Warning, TEXT("══════════════════════════════════════════════════"));
    UE_LOG(LogTemp, Warning, TEXT("✅ [ENEMY %s] EXECUTE COMPLETE"), *GetName());
    UE_LOG(LogTemp, Warning, TEXT("══════════════════════════════════════════════════\n"));
}


void AEnemyCharacter::TestDirectDamage(float Damage)
{
    if (!PlayerCharacter)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY] TestDirectDamage: No player"));
        return;
    }

    // Hit detection
    FVector Start = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    FVector End = Start + (Forward * 200.f); // 200 units forward

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(75.f),
        QueryParams
    );

    if (bShowDebug)
    {
        // ✅ DRAW DEBUG SPHERE
        DrawDebugSphere(
            GetWorld(),
            bHit ? HitResult.Location : End,
            75.f,
            12,
            bHit ? FColor::Red : FColor::Green,
            false,
            2.0f,
            0,
            3.0f
        );

        // ✅ DRAW DEBUG LINE
        DrawDebugLine(
            GetWorld(),
            Start,
            End,
            bHit ? FColor::Red : FColor::Yellow,
            false,
            2.0f,
            0,
            2.0f
        );
    }

    if (bHit && HitResult.GetActor() == PlayerCharacter)
    {
        // ✅ HIT PLAYER!
        if (IDamagable* DamagableTarget = Cast<IDamagable>(PlayerCharacter))
        {
            FDamageSpec DamageSpec;
            DamageSpec.Amount = Damage;
            DamageSpec.HitLocation = HitResult.ImpactPoint;
            DamageSpec.HitNormal = HitResult.ImpactNormal;
            DamageSpec.HitBone = HitResult.BoneName;
            DamageSpec.InstigatorController = GetController();
            DamageSpec.DamageCauser = this;
            
            IDamagable::Execute_ReceiveDamage(PlayerCharacter, DamageSpec);
            
            // ✅ NOTIFY GAME MODE
            NotifyEnemyHit(Damage);
            
            UE_LOG(LogTemp, Error, TEXT("🎯🎯🎯 ENEMY HIT PLAYER for %.1f damage 🎯🎯🎯"), Damage);
        }
    }
    else
    {
        // ✅ MISS
        ResetEnemyCombo();
        UE_LOG(LogTemp, Error, TEXT("❌❌❌ ENEMY ATTACK MISSED ❌❌❌"));
    }
}
//==============================================================================================
// MONTAGE ENDED HANDLER
//==============================================================================================
void AEnemyCharacter::OnMontageEndedHandler(FName MontageName)
{
	UE_LOG(LogTemp, Display, TEXT("🎬 [ENEMY %s] Montage ended: %s"), *GetName(), *MontageName.ToString());
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		UE_LOG(LogTemp, Display, TEXT("✅ [ENEMY %s] Ready for next action"), *GetName());
	}
}

//==============================================================================================
// ON DAMAGE TAKEN HANDLER
//==============================================================================================
void AEnemyCharacter::OnDamageTakenHandler(float DamageAmount, FVector HitLocation)
{
	UE_LOG(LogTemp, Error, TEXT("💥 [ENEMY %s] Took %.1f damage at: %s"),
		*GetName(),
		DamageAmount,
		*HitLocation.ToString());
}
FName AEnemyCharacter::SelectRandomFirstMove() 
{
	if (FirstMoveVariants.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [ENEMY] No first move variants!"));
		return FName("EnemyAttack"); // Fallback
	}
    
	int32 RandomIndex = FMath::RandRange(0, FirstMoveVariants.Num() - 1);
	FName SelectedMove = FirstMoveVariants[RandomIndex];
    
	UE_LOG(LogTemp, Warning, TEXT("🎲 [ENEMY] Random first move: %s"), 
		   *SelectedMove.ToString());
    
	return SelectedMove;
}

bool AEnemyCharacter::IsExecutingMove() const
{
	return CombatAnimationComponent && CombatAnimationComponent->IsExecutingMove();
}

	void AEnemyCharacter::Landed(const FHitResult& Hit)
	{
		Super::Landed(Hit);
    
		// ✅ Reset double jump flag when landing
		bHasDoubleJumped = false;
	}
void AEnemyCharacter::NotifyEnemyHit(float DamageDealt)
{
	AGameModeBase* GameMode = UGameplayStatics::GetGameMode(GetWorld());
	if (GameMode)
	{
		UFunction* RecordHitFunc = GameMode->FindFunction(FName("RecordEnemyHit"));
		if (RecordHitFunc)
		{
			struct FRecordHitParams
			{
				float Damage;
			};
            
			FRecordHitParams Params;
			Params.Damage = DamageDealt;
            
			GameMode->ProcessEvent(RecordHitFunc, &Params);
			UE_LOG(LogTemp, Log, TEXT("✅ Enemy hit recorded: %.1f damage"), DamageDealt);
		}
	}
}

void AEnemyCharacter::ResetEnemyCombo()
{
	AGameModeBase* GameMode = UGameplayStatics::GetGameMode(GetWorld());
	if (GameMode)
	{
		UFunction* ResetFunc = GameMode->FindFunction(FName("ResetEnemyCombo"));
		if (ResetFunc)
		{
			GameMode->ProcessEvent(ResetFunc, nullptr);
		}
	}
}
