// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include"HealthComponent.h"
#include "InputAction.h"
#include "Engine/World.h"
#include "SideScrollingInteractable.h"
#include "Kismet/KismetMathLibrary.h"
#include "TimerManager.h"
#include"CombatStateComponent.h"
#include "CombatAnimationComponent.h"
#include"Kismet/GameplayStatics.h"
#include"GameFramework/GameModeBase.h"
#include "EnemyCharacter.h"  // For enemy class check
#include "DrawDebugHelpers.h" 
#include "CombatDecisionEngine.h"
#include"CombatFeedbackComponent.h"
ASideScrollingCharacter::ASideScrollingCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	// create the camera component
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);

	Camera->SetRelativeLocationAndRotation(FVector(0.0f, 300.0f, 0.0f), FRotator(0.0f, -90.0f, 0.0f));

	// configure the collision capsule
	GetCapsuleComponent()->SetCapsuleSize(35.0f, 90.0f);

	// configure the Pawn properties
	bUseControllerRotationYaw = false;

	// configure the character movement component
	GetCharacterMovement()->GravityScale = 1.75f;
	GetCharacterMovement()->MaxAcceleration = 1500.0f;
	GetCharacterMovement()->BrakingFrictionFactor = 1.0f;
	GetCharacterMovement()->bUseSeparateBrakingFriction = true;
	GetCharacterMovement()->Mass = 500.0f;

	GetCharacterMovement()->SetWalkableFloorAngle(75.0f);
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->bIgnoreBaseRotation = true;

	GetCharacterMovement()->PerchRadiusThreshold = 15.0f;
	GetCharacterMovement()->LedgeCheckThreshold = 6.0f;

	GetCharacterMovement()->JumpZVelocity = 750.0f;
	GetCharacterMovement()->AirControl = 1.0f;

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 750.0f, 0.0f);
	GetCharacterMovement()->bOrientRotationToMovement = true;

	GetCharacterMovement()->SetPlaneConstraintNormal(FVector(0.0f, 1.0f, 0.0f));
	GetCharacterMovement()->bConstrainToPlane = true;

	// enable double jump and coyote time
	JumpMaxCount = 4;

	HealthComp = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComp"));
	CombatAnimComp = CreateDefaultSubobject<UCombatAnimationComponent>(TEXT("CombatAnimComp"));
	CombatStateComp = CreateDefaultSubobject<UCombatStateComponent>(TEXT("CombatStateComp"));
	CombatFeedbackComponent = CreateDefaultSubobject<UCombatFeedbackComponent>(TEXT("CombatFeedbackComp"));
	Tags.Add("Player.Character");
	Tags.Add(FName("Player")); // ✅ Matches HealthComponent Dojo check

}

void ASideScrollingCharacter::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// clear the wall jump timer
	GetWorld()->GetTimerManager().ClearTimer(WallJumpTimer);
}

void ASideScrollingCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ASideScrollingCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ASideScrollingCharacter::DoJumpEnd);

		// Interacting
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::DoInteract);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::Move);

		// Dropping from platform
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Triggered, this, &ASideScrollingCharacter::Drop);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Completed, this, &ASideScrollingCharacter::DropReleased);

		// Attack Action
		EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started, this, &ASideScrollingCharacter::LightAttack);
		EnhancedInputComponent->BindAction(HeavyAttackAction,ETriggerEvent::Started,this,&ASideScrollingCharacter::HeavyAttack);
		//Dash Action
		EnhancedInputComponent->BindAction(DashAction,ETriggerEvent::Started,this,&ASideScrollingCharacter::Dash);
	}
}

void ASideScrollingCharacter::NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	// only apply push impulse if we're falling
	if (!GetCharacterMovement()->IsFalling())
	{
		return;
	}

	// ensure the colliding component is valid
	if (OtherComp)
	{
		// ensure the component is movable and simulating physics
		if (OtherComp->Mobility == EComponentMobility::Movable && OtherComp->IsSimulatingPhysics())
		{
			const FVector PushDir = FVector(ActionValueY > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);

			// push the component away
			OtherComp->AddImpulse(PushDir * JumpPushImpulse, NAME_None, true);
		}
	}
}

void ASideScrollingCharacter::Landed(const FHitResult& Hit)
{
	// reset the double jump
	bHasDoubleJumped = false;
}

void ASideScrollingCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode /*= 0*/)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	// are we falling?
	if (GetCharacterMovement()->MovementMode == EMovementMode::MOVE_Falling)
	{
		// save the game time when we started falling, so we can check it later for coyote time jumps
		LastFallTime = GetWorld()->GetTimeSeconds();
	}
}

void ASideScrollingCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MoveVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MoveVector.Y);
}

void ASideScrollingCharacter::Drop(const FInputActionValue& Value)
{
	// route the input
	DoDrop(Value.Get<float>());
}

void ASideScrollingCharacter::DropReleased(const FInputActionValue& Value)
{
	// reset the input
	DoDrop(0.0f);
}

void ASideScrollingCharacter::DoMove(float Forward)
{
	// is movement temporarily disabled after wall jumping?
	if (!bHasWallJumped)
	{
		// save the movement values
		ActionValueY = Forward;

		// figure out the movement direction
		const FVector MoveDir = FVector(1.0f, Forward > 0.0f ? 0.1f : -0.1f, 0.0f);

		// apply the movement input
		AddMovementInput(MoveDir, Forward);
	}
}


void ASideScrollingCharacter::DoDrop(float Value)
{
	// save the movement value
	DropValue = Value;
}

void ASideScrollingCharacter::DoJumpStart()
{
	// handle advanced jump behaviors
	MultiJump();
}

void ASideScrollingCharacter::DoJumpEnd()
{
	StopJumping();
}

void ASideScrollingCharacter::DoInteract()
{
	// do a sphere trace to look for interactive objects
	FHitResult OutHit;

	const FVector Start = GetActorLocation();
	const FVector End = Start + FVector(100.0f, 0.0f, 0.0f);

	FCollisionShape ColSphere;
	ColSphere.SetSphere(InteractionRadius);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepSingleByObjectType(OutHit, Start, End, FQuat::Identity, ObjectParams, ColSphere, QueryParams))
	{
		// have we hit an interactable?
		if (ISideScrollingInteractable* Interactable = Cast<ISideScrollingInteractable>(OutHit.GetActor()))
		{
			// interact
			Interactable->Interaction(this);
		}
	}
}

void ASideScrollingCharacter::BeginPlay()
{
	Super::BeginPlay();
	// ═════════════════════════════════════════════════════════
	// BIND HEALTH COMPONENT DELEGATES
	// ═════════════════════════════════════════════════════════
	
	// ✅ Create UObject DecisionEngine in BeginPlay
	DecisionEngine = NewObject<UCombatDecisionEngine>(this, UCombatDecisionEngine::StaticClass());
    
	if (HealthComp)
	{
		HealthComp->OnHealthChanged.AddDynamic(this, &ASideScrollingCharacter::OnHealthChanged);
		HealthComp->OnHealthDepleted.AddDynamic(this, &ASideScrollingCharacter::OnDeath);
		// ✅ ADD THIS: Respond to damage events
		HealthComp->OnDamageTaken.AddDynamic(this, &ASideScrollingCharacter::OnDamageTakenHandler);
	}

	// ═════════════════════════════════════════════════════════
	// BIND COMBAT ANIMATION COMPONENT DELEGATES
	// ═════════════════════════════════════════════════════════
	if (CombatAnimComp)
	{
		// ✅ ADD THIS NEW LINE:
		// Listen for the "Start/Stop" signal from the component
		CombatAnimComp->OnHitWindowChanged.AddDynamic(this, &ASideScrollingCharacter::OnHitWindowChanged);
        
		CombatAnimComp->OnMontageEnded.AddDynamic(this, &ASideScrollingCharacter::OnMoveCompleted);
	}

	// ═════════════════════════════════════════════════════════
	// AUTO-FIND ENEMY FOR COMBAT STATE COMPONENT
	// ═════════════════════════════════════════════════════════
	// TEMPORARY - Test without enemy
	

	if (CombatStateComp)
	{
		TArray<AActor*> FoundEnemies;
		UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Enemy"), FoundEnemies);
		if (FoundEnemies.Num() > 0)
		{
			if (ACharacter* Enemy = Cast<ACharacter>(FoundEnemies[0]))
			{
				CombatStateComp->SetEnemy(Enemy);
				UE_LOG(LogTemp, Log, TEXT("Player found enemy: %s"), *Enemy->GetName());
			}
		}
	}
	
	if (DecisionEngine)
	{
		DecisionEngine->MoveDataTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Variant_SideScrolling/Blueprints/Combat/DT_MoveTable.DT_MoveTable"));
	}
	if (DecisionEngine->MoveDataTable)
        {
            UE_LOG(LogTemp, Log, TEXT("Decision Engine initialized with %d moves"),
                DecisionEngine->MoveDataTable->GetRowNames().Num());
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Failed to load DT_CombatMoves!"));
        }
	bIsAttacking = false;
	bIsInCombo = false;
	bCanBufferInput = false;
	bHasBufferedInput = false;
	ComboCounter = 0;
	CurrentComboStarter = FName();
	BufferedInput = FName();
    
	// Clear any pending timers
	GetWorldTimerManager().ClearTimer(ComboWindowTimer);
	GetWorldTimerManager().ClearTimer(StunTimer);
    
	UE_LOG(LogTemp, Log, TEXT("✅ Combat state reset on BeginPlay"));
}
void ASideScrollingCharacter::OnDamageTakenHandler(float Damage, FVector HitLocation)
{
	UE_LOG(LogTemp, Warning, TEXT("💥 Player took %.1f damage"), Damage);
    
	// Reset combat state (character-level logic)
	bIsInCombo = false;
	bIsAttacking = false;
	bCanBufferInput = false;
	bHasBufferedInput = false;
    
	if (CombatAnimComp)
	{
		CombatAnimComp->StopCurrentAction();
	}
}
void ASideScrollingCharacter::OnHitWindowActive(float Damage, float stun)
{
    // ═════════════════════════════════════════════════════════
    // HIT DETECTION VIA SPHERE TRACE
    // ═════════════════════════════════════════════════════════
    FVector Start = GetActorLocation();
    FVector Forward = GetActorForwardVector();
    FVector End = Start + (Forward * 150.f); // 150 units forward

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    // Sphere sweep for hit detection
    bool bHit = GetWorld()->SweepSingleByChannel(
        HitResult,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn, // Hit pawns only
        FCollisionShape::MakeSphere(75.f), // 75 unit radius
        QueryParams
    );
if (bShowDebug)
{
	// ✅ DRAW DEBUG SPHERE - Shows attack hitbox
	DrawDebugSphere(
		GetWorld(),
		bHit ? HitResult.Location : End,
		75.f,
		12,
		bHit ? FColor::Red : FColor::Green,  // Red if hit, Green if miss
		false,
		2.0f,  // Duration in seconds
		0,
		3.0f   // Thickness
	);

	// ✅ DRAW DEBUG LINE - Shows attack direction
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
   

    if (bHit && HitResult.GetActor())
    {
        AActor* Target = HitResult.GetActor();
if (bShowDebug)
{
	// ✅ DRAW DEBUG POINT - Shows exact hit location
	DrawDebugPoint(
		GetWorld(),
		HitResult.ImpactPoint,
		15.0f,
		FColor::Orange,
		false,
		2.0f
	);

}
        
        // Check if target implements IDamagable
        if (IDamagable* DamagableTarget = Cast<IDamagable>(Target))
        {
            // Build damage specification
            FDamageSpec DamageSpec;
            DamageSpec.Amount = Damage;
            DamageSpec.HitLocation = HitResult.ImpactPoint;
            DamageSpec.HitNormal = HitResult.ImpactNormal;
            DamageSpec.HitBone = HitResult.BoneName;
            DamageSpec.InstigatorController = GetController();
            DamageSpec.DamageCauser = this;

            // Apply damage through interface
            IDamagable::Execute_ReceiveDamage(Target, DamageSpec);

            // ✅ NOTIFY GAME MODE - HIT SUCCESSFULLY LANDED!
            NotifyPlayerHit(Damage);



        	///////////////////////////////////////////
//JUICE 
        	// 1. Slow down time heavily (or pause anims)
        	CustomTimeDilation = 0.01f; 
        	// Do the same for the enemy you hit
        	if (ACharacter* Enemy = Cast<ACharacter>(HitResult.GetActor()))
        	{
        		Enemy->CustomTimeDilation = 0.01f;
        	}

        	// 2. Set a Timer to reset it after 0.1 seconds (The "Stop" duration)
        	FTimerHandle HitStopTimer;
        	GetWorld()->GetTimerManager().SetTimer(HitStopTimer, [this, Target]()
			{
				// Reset Player
				this->CustomTimeDilation = 1.0f;
				// Reset Enemy
				if (Target) Target->CustomTimeDilation = 1.0f;
			}, 0.1f, false);


        	/////////////////////////////////////
        	
            UE_LOG(LogTemp, Warning, TEXT("🎯 PLAYER HIT %s for %.1f damage"), 
                   *Target->GetName(), Damage);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Hit actor but not Damagable: %s"), 
                   *Target->GetName());
        }
    }
    else
    {
        // ✅ MISS - Reset combo
       ResetPlayerCombo();
        UE_LOG(LogTemp, Error, TEXT("❌ PLAYER ATTACK MISSED - combo reset"));
    }
}


void ASideScrollingCharacter::OnMoveCompleted(FName CompletedMove)
{
	UE_LOG(LogTemp, Log, TEXT("✅ Move completed: %s"), *CompletedMove.ToString());
    
	// Open combo window
	bCanBufferInput = true;
    
	// Set timer to close combo window
	GetWorldTimerManager().SetTimer(
		ComboWindowTimer, 
		this, 
		&ASideScrollingCharacter::CloseComboWindow, 
		ComboWindowDuration, 
		false
	);
    
	UE_LOG(LogTemp, Log, TEXT("⏱️ Combo window opened for %.2f seconds"), ComboWindowDuration);
}

void ASideScrollingCharacter::StartBlocking()
{
	if (HealthComp && !HealthComp->IsStunned())
	{
		bIsBlocking = true;
		BlockStartTime = GetWorld()->GetTimeSeconds();
		// Optional: Play "Hold Block" animation here
		
	}
}

void ASideScrollingCharacter::StopBlocking()
{
	bIsBlocking = false;
	// Optional: Stop animation
}

void ASideScrollingCharacter::CloseComboWindow()
{
	bCanBufferInput = false;
    
	// Check if player buffered a follow-up input
	if (bHasBufferedInput)
	{
		// Execute buffered move via Decision Engine
		ComboCounter++;
        
		UE_LOG(LogTemp, Log, TEXT("🔗 Executing buffered combo move: %s (Hit #%d)"), 
			*BufferedInput.ToString(), ComboCounter);
        
		ExecuteMove(BufferedInput);
        
		// Clear buffer
		bHasBufferedInput = false;
		BufferedInput = FName();
	}
	else
	{
		// No follow-up input - reset combo state
		bIsInCombo = false;
		bIsAttacking = false;
		ComboCounter = 0;
		CurrentComboStarter = FName();
        
		UE_LOG(LogTemp, Log, TEXT("🔄 Combo ended - no buffered input"));
	}
}


void ASideScrollingCharacter::MultiJump()
{
	// does the user want to drop to a lower platform?
	if (DropValue > 0.0f)
	{
		CheckForSoftCollision();
		return;
	}

	// reset the drop value
	DropValue = 0.0f;

	// if we're grounded, disregard advanced jump logic
	if (!GetCharacterMovement()->IsFalling())
	{
		Jump();
		return;
	}

	// if we have a horizontal input, try for wall jump first
	if (!bHasWallJumped && !FMath::IsNearlyZero(ActionValueY))
	{
		// trace ahead of the character for walls
		FHitResult OutHit;

		const FVector Start = GetActorLocation();
		const FVector End = Start + (FVector(ActionValueY > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f) * WallJumpTraceDistance);

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		GetWorld()->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, QueryParams);

		if (OutHit.bBlockingHit)
		{
			// rotate to the bounce direction
			const FRotator BounceRot = UKismetMathLibrary::MakeRotFromX(OutHit.ImpactNormal);
			SetActorRotation(FRotator(0.0f, BounceRot.Yaw, 0.0f));

			// calculate the impulse vector
			FVector WallJumpImpulse = OutHit.ImpactNormal * WallJumpHorizontalImpulse;
			WallJumpImpulse.Z = GetCharacterMovement()->JumpZVelocity * WallJumpVerticalMultiplier;

			// launch the character away from the wall
			LaunchCharacter(WallJumpImpulse, true, true);

			// enable wall jump lockout for a bit
			bHasWallJumped = true;

			// schedule wall jump lockout reset
			GetWorld()->GetTimerManager().SetTimer(WallJumpTimer, this, &ASideScrollingCharacter::ResetWallJump, DelayBetweenWallJumps, false);

			return;
		}
	}



	// test for double jump only if we haven't already tested for wall jump
	if (!bHasWallJumped)
	{
		// are we still within coyote time frames?
		if (GetWorld()->GetTimeSeconds() - LastFallTime < MaxCoyoteTime)
		{
			UE_LOG(LogTemp, Warning, TEXT("Coyote Jump"));

			// use the built-in CMC functionality to do the jump
			Jump();

		// no coyote time jump
		} else {

			// The movement component handles double jump but we still need to manage the flag for animation
			if (!bHasDoubleJumped)
			{
				// raise the double jump flag
				bHasDoubleJumped = true;

				// let the CMC handle jump
				Jump();
			}
		}
	}
}

void ASideScrollingCharacter::CheckForSoftCollision()
{
	// reset the drop value
	DropValue = 0.0f;

	// trace down
	FHitResult OutHit;

	const FVector Start = GetActorLocation();
	const FVector End = Start + (FVector::DownVector * SoftCollisionTraceDistance);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(SoftCollisionObjectType);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	GetWorld()->LineTraceSingleByObjectType(OutHit, Start, End, ObjectParams, QueryParams);

	// did we hit a soft floor?
	if (OutHit.GetActor())
	{
		// drop through the floor
		SetSoftCollision(true);
	}
}

void ASideScrollingCharacter::ResetWallJump()
{
	// reset the wall jump flag
	bHasWallJumped = false;
}

void ASideScrollingCharacter::OnHealthChanged(float Current, float Max)
{
//	UE_LOG(LogTemp, Log, TEXT("Player Health: %.1f / %.1f"), Current, Max);
}

void ASideScrollingCharacter::OnDeath()
{
//	UE_LOG(LogTemp, Warning, TEXT("Player died!"));
	BP_HandleDeath();
}

void ASideScrollingCharacter::ReceiveDamage_Implementation(const FDamageSpec& Spec)
{
	if (!HealthComp || !HealthComp->IsAlive())
		return;


	
	if (bShowDebug)
	{
		// ✅ DRAW DEBUG SPHERE - Player taking damage
		DrawDebugSphere(
			GetWorld(),
			GetActorLocation(),
			80.0f,
			12,
			FColor::Magenta,  // Purple = player damaged
			false,
			2.0f,
			0,
			4.0f
		);

		// ✅ DRAW DAMAGE DIRECTION
		DrawDebugLine(
			GetWorld(),
			Spec.HitLocation,
			GetActorLocation(),
			FColor::Cyan,
			false,
			2.0f,
			0,
			3.0f
		);
	}

	// 1. CHECK BLOCKING
	// We only block if holding the button AND facing the attacker
	if (bIsBlocking && IsFacing(Spec.DamageCauser)) 
	{
		float BlockDuration = GetWorld()->GetTimeSeconds() - BlockStartTime;

		// --- PERFECT PARRY (Pressed < 0.2s ago) ---
		if (BlockDuration < 0.25f)
		{
			// JUICE: Blue Sparks + Clang Sound
			if (CombatFeedbackComponent)
			{
				// Assuming you added BlockVFX to your component, or just re-use LightHit with 0 damage
				CombatFeedbackComponent->PlayImpactFeedback(Spec.HitLocation, Spec.HitNormal, 0.0f, this); 
			}

			// REVERSAL: Stun the Enemy!
			if (ACharacter* Attacker = Cast<ACharacter>(Spec.DamageCauser))
			{
				if (UHealthComponent* EnemyHP = Attacker->FindComponentByClass<UHealthComponent>())
				{
					EnemyHP->DamagePoise(100.0f); // Instantly break their guard
				}
			}
			// Take NO damage
			return; 
		}

		// --- NORMAL BLOCK ---
		// Take Chip Damage to Poise
		if (HealthComp)
		{
			// If block breaks, we get stunned
			bool bGuardBroken = HealthComp->DamagePoise(20.0f); 
			if (bGuardBroken)
			{
				StopBlocking();
			}
		}

		// Reduce HP damage by 80%
		float ReducedDamage = Spec.Amount * 0.2f;
        
		// Pass reduced damage to normal logic
		if (HealthComp) HealthComp->ApplyDamage(ReducedDamage, Spec.DamageType, Spec.HitLocation,Spec.DamageCauser);
		return;
	}
	if (CombatFeedbackComponent)
	{
		// This now matches the signature perfectly (4 args)
		CombatFeedbackComponent->PlayImpactFeedback(
			Spec.HitLocation, 
			Spec.HitNormal, 
			Spec.Amount, 
			Spec.DamageCauser 
		);
	}

	// ✅ SIMPLE: Just forward to HealthComponent
	HealthComp->ApplyDamage(Spec.Amount, EDamageType::Physical, Spec.HitLocation,Spec.DamageCauser);

	// ✅ CHECK IF DAMAGE CAME FROM ENEMY (using tags)
	if (Spec.DamageCauser && Spec.DamageCauser->Tags.Contains(FName("Enemy.Character")))
	{
		// Notify game mode
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
				Params.Damage = Spec.Amount;
                
				GameMode->ProcessEvent(RecordHitFunc, &Params);
			}
		}
	}
}

void ASideScrollingCharacter::OnHitWindowChanged(bool bActive, const FActionCommand& Command)
{
	bIsHitboxActive = bActive;

	if (bActive)
	{
		CurrentHitDamage = Command.DamageToApply;
		HitActors.Empty(); // Reset for new swing
	}
}

void ASideScrollingCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bIsHitboxActive)
	{
		PerformAttackTrace();
	}
}

// ✅ NEW: The Physics Logic
void ASideScrollingCharacter::PerformAttackTrace()
{
	FVector Start = GetActorLocation();
	FVector Forward = GetActorForwardVector();
	FVector End = Start + (Forward * 80.f); // 120 units forward reach

	
	TArray<FHitResult> OutHits;
	TArray<AActor*> Ignored;
	Ignored.Add(this);

	// Sphere Trace
	bool bHit = UKismetSystemLibrary::SphereTraceMulti(
		GetWorld(), 
		Start, 
		End, 
		63.f, // Radius
		UEngineTypes::ConvertToTraceType(ECC_Pawn),
		false, 
		Ignored,
		bShowDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		OutHits, 
		true
	);

	if (bHit)
	{
		for (const FHitResult& Hit : OutHits)
		{
			AActor* Target = Hit.GetActor();
			
			// Valid target? Has Interface? Not hit yet this swing?
			if (Target && Target->Implements<UDamagable>() && !HitActors.Contains(Target))
			{
				HitActors.Add(Target); // Mark as hit so we don't hit them next frame

				// Construct Damage Spec
				FDamageSpec Spec;
				Spec.Amount = CurrentHitDamage;
				Spec.HitLocation = Hit.ImpactPoint;
				Spec.HitNormal = Hit.ImpactNormal;
				Spec.DamageCauser = this; // ✅ I am the causer
				Spec.InstigatorController = GetController();

				// Send Damage
				IDamagable::Execute_ReceiveDamage(Target, Spec);
				
				// Reward Player (Juice)
				NotifyPlayerHit(CurrentHitDamage); 
			}
		}
	}
}

bool ASideScrollingCharacter::IsFacing(AActor* OtherActor)
{
	if (!OtherActor) return true;
	FVector ToTarget = OtherActor->GetActorLocation() - GetActorLocation();
	float Dot = FVector::DotProduct(GetActorForwardVector(), ToTarget.GetSafeNormal());
	return Dot > 0.0f; // Positive dot product means it's in front of us
}
void ASideScrollingCharacter::OnStunExpired()
{
	if (HealthComp)
	{
		HealthComp->StunDuration = 0.0f;  // ✅ Direct access to public variable
	//	UE_LOG(LogTemp, Log, TEXT("✅ Player stun cleared - can act again"));
	}
}
bool ASideScrollingCharacter::IsStunned() const
{
	return HealthComp && HealthComp->StunDuration > 0.0f;
}

// Clean separation of concerns
void ASideScrollingCharacter::ExecuteMove(FName Input)
{
 // 1. Validation
	if (!CombatStateComp || !DecisionEngine || !CombatAnimComp)
	{
		UE_LOG(LogTemp, Error, TEXT("❌ [PLAYER] Missing Components!"));
		return;
	}

	// 2. Build Context & Decide
	FContextVector Context = CombatStateComp->BuildContext(Input);
	FActionCommand Command = DecisionEngine->DecideNextMove(Context);
	
	// 3. Execute Animation
	// The Component will now manage the timer and call OnHitWindowChanged
	CombatAnimComp->ExecuteActionCommand(Command);

	// 4. Set Cooldowns
	CombatStateComp->StartCooldown(Command.MoveIdentifier, 0.5f);
}



bool ASideScrollingCharacter::IsAlive_Implementation() const
{
	return HealthComp && HealthComp->IsAlive();
}

void ASideScrollingCharacter::SetSoftCollision(bool bEnabled)
{
	// enable or disable collision response to the soft collision channel
	GetCapsuleComponent()->SetCollisionResponseToChannel(SoftCollisionObjectType, bEnabled ? ECR_Ignore : ECR_Block);
}

bool ASideScrollingCharacter::HasDoubleJumped() const
{
	return bHasDoubleJumped;
}

bool ASideScrollingCharacter::HasWallJumped() const
{
	return bHasWallJumped;
}


void ASideScrollingCharacter::LightAttack(const FInputActionValue& Value)
{
	// Player presses Light Attack button
	// This is the INITIAL INPUT the engine sees
	if (IsStunned())
	{
	//	UE_LOG(LogTemp, Warning, TEXT("⚡ Cannot attack - stunned!"));
		return;
	}
	
	if (!bIsAttacking)
	{
		TArray<FName> LightAttackVariants = {
			FName("LightAttack"),
			FName("LightAttack3"),
			FName("LightAttack2"),
			FName("LightAttack4"),
			FName("LightAttack1"),
			FName("LightAttack5")
		};
		int32 RandomIndex = FMath::RandRange(0, LightAttackVariants.Num() - 1);
		FName SelectedVariant = LightAttackVariants[RandomIndex];
	
		ExecuteMove(SelectedVariant);
		bIsAttacking = true;
	}
	
}

void ASideScrollingCharacter::HeavyAttack(const FInputActionValue& Value)
{
	if (!bIsAttacking)
	{
		ExecuteMove(FName("HeavyAttack"));
		bIsAttacking = true;
	}
}

void ASideScrollingCharacter::Dash(const FInputActionValue& Value)
{
	ExecuteMove(FName("Dash"));
}
void ASideScrollingCharacter::NotifyPlayerHit(float DamageDealt)
{
	// Get the game mode
	AGameModeBase* GameMode = UGameplayStatics::GetGameMode(GetWorld());
	if (GameMode)
	{
		// Call Blueprint event on game mode
		// Your BP Game Mode should have a custom event called "RecordPlayerHit"
		UFunction* RecordHitFunc = GameMode->FindFunction(FName("RecordPlayerHit"));
		if (RecordHitFunc)
		{
			struct FRecordHitParams
			{
				float Damage;
			};
            
			FRecordHitParams Params;
			Params.Damage = DamageDealt;
            
			GameMode->ProcessEvent(RecordHitFunc, &Params);
			//UE_LOG(LogTemp, Log, TEXT("✅ Player hit recorded: %.1f damage"), DamageDealt);
		}
	}
}

void ASideScrollingCharacter::ResetPlayerCombo()
{
	AGameModeBase* GameMode = UGameplayStatics::GetGameMode(GetWorld());
	if (GameMode)
	{
		UFunction* ResetFunc = GameMode->FindFunction(FName("ResetPlayerCombo"));
		if (ResetFunc)
		{
			GameMode->ProcessEvent(ResetFunc, nullptr);
			//UE_LOG(LogTemp, Log, TEXT("✅ Player combo reset"));
		}
	}
}

 