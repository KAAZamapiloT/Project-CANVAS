// Fill out your copyright notice in the Description page of Project Settings.



// Implementation of the combat animation executor

#include "CombatAnimationComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"

UCombatAnimationComponent::UCombatAnimationComponent()
{
    // Disable tick - we're event-driven via montage delegates
    PrimaryComponentTick.bCanEverTick = false;
    
    // Initialize cached values
    CachedDamage = 0.f;
    CachedStunDuration = 0.f;
    MoveStartTime = 0.f;
}

void UCombatAnimationComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Cache references to owner and anim instance
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
    }
    
    // Validate setup
    if (!AnimInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatAnimationComponent: Failed to get AnimInstance from owner"));
    }
}

void UCombatAnimationComponent::ExecuteActionCommand(const FActionCommand& Command)
{
    // Validate inputs
    if (!AnimInstance || !Command.AnimationToPlay)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExecuteActionCommand: Invalid AnimInstance or Montage"));
        return;
    }

    // Cache command data for AnimNotify callbacks
    // When AnimNotify_CombatHit fires, we'll use these cached values
    CurrentMoveIdentifier = Command.MoveIdentifier;
    CachedDamage = Command.DamageToApply;
    CachedStunDuration = Command.StunDurationToInflict;
    MoveStartTime = GetWorld()->GetTimeSeconds();

    // Bind delegate to handle montage completion
    // This triggers combo chaining and state cleanup
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UCombatAnimationComponent::OnMontageCompleted);

    // Play the montage at normal speed (1.0 playrate)
    AnimInstance->Montage_Play(Command.AnimationToPlay, 1.0f);
    
    // Set the end delegate for this specific montage
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Command.AnimationToPlay);

    UE_LOG(LogTemp, Log, TEXT("Playing move: %s (Damage: %.1f, Stun: %.1f)"), 
           *CurrentMoveIdentifier.ToString(), CachedDamage, CachedStunDuration);
}

void UCombatAnimationComponent::StopCurrentAction()
{
    // Stop any active montage with a short blend-out time
    if (AnimInstance && AnimInstance->GetCurrentActiveMontage())
    {
        AnimInstance->Montage_Stop(0.2f);  // 0.2 second blend out
    }
    
    // Clear state
    CurrentMoveIdentifier = NAME_None;
    MoveStartTime = 0.f;
    
    UE_LOG(LogTemp, Log, TEXT("Combat action stopped"));
}

void UCombatAnimationComponent::OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted)
{
    // Store the completed move identifier before clearing
    FName CompletedMove = CurrentMoveIdentifier;
    
    // Clear execution state
    CurrentMoveIdentifier = NAME_None;
    MoveStartTime = 0.f;

    // Broadcast to listeners (usually character class for combo chaining)
    // Listeners can check buffered input and query Decision Engine for next move
    OnMontageEnded.Broadcast(CompletedMove);
    
    UE_LOG(LogTemp, Log, TEXT("Move ended: %s (interrupted: %d)"), 
           *CompletedMove.ToString(), bInterrupted);
}

float UCombatAnimationComponent::GetMoveElapsedTime() const
{
    // Return 0 if no move is active
    if (CurrentMoveIdentifier.IsNone())
    {
        return 0.f;
    }
    
    // Calculate time since move started
    return GetWorld()->GetTimeSeconds() - MoveStartTime;
}

void UCombatAnimationComponent::HandleHitNotify()
{
    // Called by AnimNotify_CombatHit during montage playback
    // Broadcast cached damage/stun values to listeners
    // Character class will perform hit detection and apply damage here
    OnHitWindowActive.Broadcast(CachedDamage, CachedStunDuration);
    
    UE_LOG(LogTemp, Log, TEXT("Hit window active: Damage=%.1f Stun=%.1f"), 
           CachedDamage, CachedStunDuration);
}

