#include "CombatFeedbackComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"

UCombatFeedbackComponent::UCombatFeedbackComponent()
{
    PrimaryComponentTick.bCanEverTick = false; // Efficient
}

// In CombatFeedbackComponent.cpp

void UCombatFeedbackComponent::PlayImpactFeedback(FVector HitLocation, FVector HitNormal, float Damage, AActor* Instigator)
{
    // 1. DETERMINE INTENSITY
    bool bIsHeavy = Damage > 20.0f;

    // 2. SPAWN VFX (Use HitNormal for rotation)
    UNiagaraSystem* VFXToSpawn = bIsHeavy ? HeavyHitVFX : LightHitVFX;
    if (VFXToSpawn)
    {
        FRotator Rotation = HitNormal.Rotation(); // <--- UPDATED
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFXToSpawn, HitLocation, Rotation); // <--- UPDATED
    }

    // 3. PLAY SOUND (Use HitLocation)
    USoundBase* SoundToPlay = bIsHeavy ? HeavyHitSound : LightHitSound;
    if (SoundToPlay)
    {
        UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, HitLocation); // <--- UPDATED
    }

    // 4. CAMERA SHAKE (Logic remains the same)
    APlayerController* PC = nullptr;
    
    // Case A: I (Owner) am the player and got hit
    if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
    {
        if (OwnerPawn->IsLocallyControlled()) PC = Cast<APlayerController>(OwnerPawn->GetController());
    }
    
    // Case B: Player hit something (Instigator is player)
    if (!PC && Instigator)
    {
        if (APawn* InstigatorPawn = Cast<APawn>(Instigator))
        {
            if (InstigatorPawn->IsLocallyControlled()) PC = Cast<APlayerController>(InstigatorPawn->GetController());
        }
    }

    if (PC)
    {
        TSubclassOf<UCameraShakeBase> ShakeClass = bIsHeavy ? HeavyShake : LightShake;
        if (ShakeClass) PC->ClientStartCameraShake(ShakeClass);
    }

    // 5. HIT STOP
    if (bEnableHitStop)
    {
        ApplyHitStop(GetOwner(), Instigator);
    }
}

void UCombatFeedbackComponent::ApplyHitStop(AActor* Target, AActor* Instigator)
{
    // Freeze both actors
    if (Target) Target->CustomTimeDilation = 0.001f;
    if (Instigator) Instigator->CustomTimeDilation = 0.001f;

    // Set Timer to unfreeze
    FTimerHandle TimerHandle;
    FTimerDelegate TimerDel;
    TimerDel.BindUObject(this, &UCombatFeedbackComponent::ResetTimeDilation, Target, Instigator);
    
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDel, HitStopDuration, false);
}

void UCombatFeedbackComponent::ResetTimeDilation(AActor* Target, AActor* Instigator)
{
    if (Target) Target->CustomTimeDilation = 1.0f;
    if (Instigator) Instigator->CustomTimeDilation = 1.0f;
}