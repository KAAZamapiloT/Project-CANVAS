// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include"Damagable.h"
#include "GameFramework/Character.h"
#include "SideScrollingCharacter.generated.h"

class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UHealthComponent;
class UCombatAnimationComponent;  // ADD
class UCombatStateComponent;      // ADD
class UCombatDecisionEngine;
class UCombatFeedbackComponent;
/**
 *  A player-controllable character side scrolling game
 */
UCLASS(abstract)
class ASideScrollingCharacter : public ACharacter,public IDamagable
{
	GENERATED_BODY()

	/** Player camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category ="Camera", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* Camera;

protected:

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Drop from Platform Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DropAction;

	/** Interact Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* InteractAction;

	//** Attack Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LightAttackAction;
    
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* HeavyAttackAction;
    
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* DashAction;
	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* BlockAction;
	/** Impulse to manually push physics objects while we're in midair */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Jump")
	float JumpPushImpulse = 600.0f;

	/** Max distance that interactive objects can be triggered */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Interaction")
	float InteractionRadius = 200.0f;

	/** Time to disable input after a wall jump to preserve momentum */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float DelayBetweenWallJumps = 0.3f;

	/** Distance to trace ahead of the character for wall jumps */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpTraceDistance = 50.0f;

	/** Horizontal impulse to apply to the character during wall jumps */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpHorizontalImpulse = 500.0f;

	/** Multiplies the jump Z velocity for wall jumps. */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Wall Jump")
	float WallJumpVerticalMultiplier = 1.4f;

	/** Collision object type to use for soft collision traces (dropping down floors) */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Soft Platforms")
	TEnumAsByte<ECollisionChannel> SoftCollisionObjectType;

	/** Distance to trace down during soft collision checks */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Soft Platforms")
	float SoftCollisionTraceDistance = 1000.0f;

	/** Last recorded time when this character started falling */
	float LastFallTime = 0.0f;

	/** Max amount of time that can pass since we started falling when we allow a regular jump */
	UPROPERTY(EditAnywhere, Category="Side Scrolling|Coyote Time", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float MaxCoyoteTime = 0.16f;

	/** Wall jump lockout timer */
	FTimerHandle WallJumpTimer;

	/** Last captured horizontal movement input value */
	float ActionValueY = 0.0f;

	/** Last captured platform drop axis value */
	float DropValue = 0.0f;

	/** If true, this character has already wall jumped */
	bool bHasWallJumped = false;

	/** If true, this character has already double jumped */
	bool bHasDoubleJumped = false;

	/** If true, this character is moving along the side scrolling axis */
	bool bMovingHorizontally = false;

	/**Gating attacks*/
	bool bIsAttacking = false;
    
	// ===== ADD THESE NEW VARIABLES =====
	/** If true, player is currently in a combo chain */
	bool bIsInCombo = false;
    
	/** If true, player can buffer next input during combo window */
	bool bCanBufferInput = false;
    
	/** Stores buffered input during combo window */
	FName BufferedInput;
    
	/** True if player has buffered an input */
	bool bHasBufferedInput = false;
    
	/** Tracks which move started the current combo (e.g., "LightAttack") */
	FName CurrentComboStarter;
    
	/** Number of hits in current combo chain */
	int32 ComboCounter = 0;
    
	/** Timer handle for combo window duration */
	FTimerHandle ComboWindowTimer;
    
	/** Duration of combo window in seconds */
	UPROPERTY(EditAnywhere, Category="Combat|Combo")
	float ComboWindowDuration = 0.3f;
	/** Timer handle for stun duration */
	FTimerHandle StunTimer;
	UPROPERTY(EditAnywhere, Category="Health")
	class UHealthComponent* HealthComp;
	// Add these UPROPERTYs in protected section (after HealthComp)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	UCombatAnimationComponent* CombatAnimComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	UCombatStateComponent* CombatStateComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	class UCombatDecisionEngine* DecisionEngine;


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UCombatFeedbackComponent* CombatFeedbackComponent;
public:
	
	/** Constructor */
	ASideScrollingCharacter();
	UFUNCTION(BlueprintCallable)
	void OnDamageTakenHandler(float Damage, FVector HitLocation);
protected:

	/** Closes the combo window and executes buffered input if any */
	void CloseComboWindow();

	/** Called when stun duration expires */
	void OnStunExpired();
    
	/** Check if player is currently stunned */
	UFUNCTION(BlueprintPure, Category="Combat|State")
	bool IsStunned() const;
	/** Gameplay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Collision handling */
	virtual void NotifyHit(class UPrimitiveComponent* MyComp, AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	/** Landing handling */
	virtual void Landed(const FHitResult& Hit) override;

	/** Handle movement mode changes to keep track of coyote time jumps */
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for drop from platform input */
	void Drop(const FInputActionValue& Value);

	/** Called for drop from platform input release */
	void DropReleased(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Forward);

	/** Handles drop inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoDrop(float Value);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Handles interact inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoInteract();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Damage")
	void OnHitWindowActive(float Damage,float stun);

	UFUNCTION()  // ✅ ADD THIS FUNCTION
	void OnMoveCompleted(FName CompletedMove);
protected:

	/** Handles advanced jump logic */
	void MultiJump();

	/** Checks for soft collision with platforms */
	void CheckForSoftCollision();

	/** Resets wall jump lockout. Called from timer after a wall jump */
	void ResetWallJump();

	UFUNCTION()
	void OnHealthChanged(float Current, float Max);

	UFUNCTION()
	void OnDeath();

	UFUNCTION(BlueprintCallable, Category="Input")
	void ExecuteMove(FName input);

	void LightAttack(const FInputActionValue& Value);
	void HeavyAttack(const FInputActionValue& Value);
	void Dash(const FInputActionValue& Value);
public:

	/** Sets the soft collision response. True passes, False blocks */
	void SetSoftCollision(bool bEnabled);

	virtual void ReceiveDamage_Implementation(const FDamageSpec& Spec) override;
	virtual bool IsAlive_Implementation() const override;
public:

	/** Returns true if the character has just double jumped */
	UFUNCTION(BlueprintPure, Category="Side Scrolling")
	bool HasDoubleJumped() const;

	/** Returns true if the character has just wall jumped */
	UFUNCTION(BlueprintPure, Category="Side Scrolling")
	bool HasWallJumped() const;

	/** Notify game mode that player landed a hit */
	UFUNCTION(BlueprintCallable, Category = "Dojo Stats")
	void NotifyPlayerHit(float DamageDealt);

	/** Reset player's current combo counter */
	UFUNCTION(BlueprintCallable, Category = "Dojo Stats")
	void ResetPlayerCombo();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dojo Stats")
	bool bShowDebug=false;

protected:
	/** Timer handle for temporary damage testing */
	FTimerHandle TempPlayerDamageTestHandle;
    
	/** Test direct damage (temporary - until AnimNotify is added) */
	void TestPlayerDirectDamage(float Damage);
        /** * Bridge function: C++ calls this, Blueprint executes it.
         * Implement this event in your BP_SideScrollingCharacter Event Graph.
         */
        UFUNCTION(BlueprintImplementableEvent, Category = "Combat")
        void BP_HandleDeath();
};
