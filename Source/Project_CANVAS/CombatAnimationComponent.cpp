#include "CombatAnimationComponent.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include"AIController.h"
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
    
    // ✅ DETECT IF THIS IS ENEMY OR PLAYER
    FString OwnerType = GetOwnerType();
    
    if (!AnimInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [%s] CombatAnimationComponent: Failed to get AnimInstance!"), *OwnerType);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("✅ [%s] CombatAnimationComponent initialized"), *OwnerType);
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
    // ✅ GET OWNER TYPE FOR LOGGING
    FString OwnerType = GetOwnerType();
    
    if (!AnimInstance || !Command.AnimationToPlay)
    {
        UE_LOG(LogTemp, Warning, TEXT("❌ [%s] ExecuteActionCommand: Invalid AnimInstance or Montage"), *OwnerType);
        return;
    }
    
    // Cache action data
    CurrentMoveIdentifier = Command.MoveIdentifier;
    CachedDamage = Command.DamageToApply;
    CachedStunDuration = Command.StunDurationToInflict;
    MoveStartTime = GetWorld()->GetTimeSeconds();

    // Bind montage completion
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UCombatAnimationComponent::OnMontageCompleted);
    
    // ✅ PLAY MONTAGE & LOG RESULT
    float Duration = AnimInstance->Montage_Play(Command.AnimationToPlay, 1.0f);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Command.AnimationToPlay);

    if (Duration > 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("🎬 [%s] Playing: %s | Damage: %.1f | Stun: %.1f | Duration: %.2fs"), 
            *OwnerType,
            *CurrentMoveIdentifier.ToString(), 
            CachedDamage, 
            CachedStunDuration,
            Duration);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ [%s] Failed to play montage: %s"), 
            *OwnerType,
            *Command.AnimationToPlay->GetName());
    }
}

void UCombatAnimationComponent::StopCurrentAction()
{
    FString OwnerType = GetOwnerType();
    
    if (AnimInstance && AnimInstance->GetCurrentActiveMontage())
    {
        AnimInstance->Montage_Stop(0.2f); // Blend out
    }
    
    CurrentMoveIdentifier = NAME_None;
    MoveStartTime = 0.f;
    
    UE_LOG(LogTemp, Log, TEXT("🛑 [%s] Combat action stopped"), *OwnerType);
}

void UCombatAnimationComponent::OnMontageCompleted(UAnimMontage* Montage, bool bInterrupted)
{
    FString OwnerType = GetOwnerType();
    FName CompletedMove = CurrentMoveIdentifier;
    
    CurrentMoveIdentifier = NAME_None;
    MoveStartTime = 0.f;
    
    OnMontageEnded.Broadcast(CompletedMove);
    
    UE_LOG(LogTemp, Log, TEXT("✅ [%s] Move ended: %s %s"), 
        *OwnerType,
        *CompletedMove.ToString(), 
        bInterrupted ? TEXT("(INTERRUPTED)") : TEXT("(completed)"));
}

float UCombatAnimationComponent::GetMoveElapsedTime() const
{
    return CurrentMoveIdentifier.IsNone() ? 0.f : GetWorld()->GetTimeSeconds() - MoveStartTime;
}

void UCombatAnimationComponent::HandleHitNotify()
{
    FString OwnerType = GetOwnerType();
    
    OnHitWindowActive.Broadcast(CachedDamage, CachedStunDuration);
    
    UE_LOG(LogTemp, Warning, TEXT("🎯 [%s] Hit window active! Damage: %.1f | Stun: %.1f"), 
        *OwnerType,
        CachedDamage, 
        CachedStunDuration);
}

// ========================================
// ✅ HELPER FUNCTION TO DETECT ENEMY/PLAYER
// ========================================
FString UCombatAnimationComponent::GetOwnerType() const
{
    if (!OwnerCharacter)
    {
        return TEXT("UNKNOWN");
    }
    
    // Check if owner is player-controlled
    if (OwnerCharacter->IsPlayerControlled())
    {
        return TEXT("PLAYER");
    }
    
    // Check if owner has AI controller (Enemy)
    if (OwnerCharacter->GetController() && OwnerCharacter->GetController()->IsA(AAIController::StaticClass()))
    {
        return FString::Printf(TEXT("ENEMY [%s]"), *OwnerCharacter->GetName());
    }
    
    return TEXT("AI");
}
