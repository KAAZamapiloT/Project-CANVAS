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
    // ✅ ENABLE TICK so we can trace every frame during attacks
    PrimaryComponentTick.bCanEverTick = true; 
    PrimaryComponentTick.bStartWithTickEnabled = false;
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

// In CombatAnimationComponent.cpp

void UCombatAnimationComponent::ExecuteActionCommand(const FActionCommand& Command)
{
    FString OwnerType = GetOwnerType();
    
    if (!AnimInstance || !Command.AnimationToPlay) return;
    
    // 1. Play Animation
    AnimInstance->Montage_Play(Command.AnimationToPlay, 1.0f);
    
    // Cache Data
    CurrentMoveIdentifier = Command.MoveIdentifier;
    CachedCommand = Command; 

    // 2. SCHEDULE THE HITBOX (This is the missing link!)
    // Use the delays defined in your Data Struct
    float Delay = FMath::Max(0.01f, Command.HitWindowDelay);
    
    // Timer to OPEN Hitbox
    GetWorld()->GetTimerManager().SetTimer(
        Timer_StartHitbox, 
        this, 
        &UCombatAnimationComponent::StartHitbox, 
        Delay, 
        false
    );

    // Timer to CLOSE Hitbox
    GetWorld()->GetTimerManager().SetTimer(
        Timer_StopHitbox, 
        this, 
        &UCombatAnimationComponent::StopHitbox, 
        Delay + Command.HitWindowDuration, 
        false
    );

    // 3. Bind Completion (Existing logic)
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &UCombatAnimationComponent::OnMontageCompleted);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Command.AnimationToPlay);
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
    
  //  UE_LOG(LogTemp, Log, TEXT("🛑 [%s] Combat action stopped"), *OwnerType);
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

void UCombatAnimationComponent::StartHitbox()
{
    OnHitWindowChanged.Broadcast(true, CachedCommand);
    // UE_LOG(LogTemp, Log, TEXT("⚔️ Hitbox OPEN"));
}

void UCombatAnimationComponent::StopHitbox()
{
    OnHitWindowChanged.Broadcast(true, CachedCommand);
    // UE_LOG(LogTemp, Log, TEXT("🛡️ Hitbox CLOSED"));
}


void UCombatAnimationComponent::HandleHitNotify()
{
    FString OwnerType = GetOwnerType();
    
    OnHitWindowActive.Broadcast(CachedDamage, CachedStunDuration);
    
 //   UE_LOG(LogTemp, Warning, TEXT("🎯 [%s] Hit window active! Damage: %.1f | Stun: %.1f"), 
 //       *OwnerType,CachedDamage, CachedStunDuration);
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
