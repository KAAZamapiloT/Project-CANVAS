#include "CombatAnimationComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"

UCombatAnimationComponent::UCombatAnimationComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Event-driven
    CachedDamage = 0.f;
    CachedStunDuration = 0.f;
    MoveStartTime = 0.f;
    OwnerCharacter = nullptr;
    AnimInstance = nullptr;
    CurrentMoveIdentifier = NAME_None;
}

void UCombatAnimationComponent::BeginPlay()
{
    Super::BeginPlay();
    OwnerCharacter = Cast<ACharacter>(GetOwner());
    AnimInstance = OwnerCharacter ? OwnerCharacter->GetMesh()->GetAnimInstance() : nullptr;
    if (!AnimInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatAnimationComponent: Failed to get AnimInstance!"));
    }
}

bool UCombatAnimationComponent::IsPlayingMontage() const
{
    return AnimInstance && AnimInstance->IsAnyMontagePlaying();
}

bool UCombatAnimationComponent::IsValidForExecution() const
{
    return AnimInstance && AnimInstance->IsValidLowLevel() && OwnerCharacter && OwnerCharacter->IsValidLowLevel();
}

void UCombatAnimationComponent::ExecuteActionCommand(const FActionCommand& Command)
{
    if (!AnimInstance || !Command.AnimationToPlay)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExecuteActionCommand: Invalid AnimInstance or Montage"));
        return;
    }
    CurrentMoveIdentifier = Command.MoveIdentifier;
    CachedDamage = Command.DamageToApply;
    CachedStunDuration = Command.StunDurationToInflict;
    MoveStartTime = GetWorld()->GetTimeSeconds();

    // Bind montage completion
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UCombatAnimationComponent::OnMontageCompleted);
    AnimInstance->Montage_Play(Command.AnimationToPlay, 1.0f);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Command.AnimationToPlay);

    UE_LOG(LogTemp, Log, TEXT("Playing move: %s (Damage: %.1f, Stun: %.1f)"), *CurrentMoveIdentifier.ToString(), CachedDamage, CachedStunDuration);
}

void UCombatAnimationComponent::StopCurrentAction()
{
    if (AnimInstance && AnimInstance->GetCurrentActiveMontage())
    {
        AnimInstance->Montage_Stop(0.2f); // Blend out
    }
    CurrentMoveIdentifier = NAME_None;
    MoveStartTime = 0.f;
    UE_LOG(LogTemp, Log, TEXT("Combat action stopped"));
}

void UCombatAnimationComponent::OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted)
{
    FName CompletedMove = CurrentMoveIdentifier;
    CurrentMoveIdentifier = NAME_None;
    MoveStartTime = 0.f;
    OnMontageEnded.Broadcast(CompletedMove);
    UE_LOG(LogTemp, Log, TEXT("Move ended: %s (interrupted: %d)"), *CompletedMove.ToString(), bInterrupted);
}

float UCombatAnimationComponent::GetMoveElapsedTime() const
{
    return CurrentMoveIdentifier.IsNone() ? 0.f : GetWorld()->GetTimeSeconds() - MoveStartTime;
}

void UCombatAnimationComponent::HandleHitNotify()
{
    OnHitWindowActive.Broadcast(CachedDamage, CachedStunDuration);
    UE_LOG(LogTemp, Log, TEXT("Hit window active: Damage=%.1f Stun=%.1f"), CachedDamage, CachedStunDuration);
}
