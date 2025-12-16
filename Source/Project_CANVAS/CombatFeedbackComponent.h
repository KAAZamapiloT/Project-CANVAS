#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NiagaraSystem.h"
#include "Camera/CameraShakeBase.h"
#include "CombatFeedbackComponent.generated.h"

UCLASS( ClassGroup=(Combat), meta=(BlueprintSpawnableComponent) )
class PROJECT_CANVAS_API UCombatFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCombatFeedbackComponent();

	/** * Universal function to play feedback. 
	 * Call this from ANY actor (Player, Enemy, Wall) when it gets hit.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat Juice")
	void PlayImpactFeedback(FVector HitLocation, FVector HitNormal, float Damage, AActor* Instigator);

protected:
	// --- VISUALS ---
	UPROPERTY(EditAnywhere, Category = "Juice|VFX")
	UNiagaraSystem* LightHitVFX;

	UPROPERTY(EditAnywhere, Category = "Juice|VFX")
	UNiagaraSystem* HeavyHitVFX;

	UPROPERTY(EditAnywhere, Category = "Juice|VFX")
	UNiagaraSystem* BlockVFX;

	// --- AUDIO ---
	UPROPERTY(EditAnywhere, Category = "Juice|SFX")
	USoundBase* LightHitSound;

	UPROPERTY(EditAnywhere, Category = "Juice|SFX")
	USoundBase* HeavyHitSound;

	// --- FEEL (Camera Shake) ---
	// Only plays if the Local Player is the Instigator or the Victim
	UPROPERTY(EditAnywhere, Category = "Juice|Camera")
	TSubclassOf<UCameraShakeBase> LightShake;

	UPROPERTY(EditAnywhere, Category = "Juice|Camera")
	TSubclassOf<UCameraShakeBase> HeavyShake;

	// --- HIT STOP ---
	UPROPERTY(EditAnywhere, Category = "Juice|HitStop")
	bool bEnableHitStop = true;

	UPROPERTY(EditAnywhere, Category = "Juice|HitStop")
	float HitStopDuration = 0.1f;

private:
	void ApplyHitStop(AActor* Target, AActor* Instigator);
	void ResetTimeDilation(AActor* Target, AActor* Instigator);
};