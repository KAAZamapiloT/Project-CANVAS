// Fill out your copyright notice in the Description page of Project Settings.

// AnimNotify_CombatHit.h
// Custom AnimNotify for frame-accurate damage application
// Place this notify in montages at the exact hit frame

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_CombatHit.generated.h"

/**
 * UAnimNotify_CombatHit
 * 
 * Custom animation notify for combat hit detection.
 * When this notify fires during a montage, it triggers the 
 * CombatAnimationComponent's HandleHitNotify() function.
 * 
 * Usage:
 * 1. Place this notify in attack montages at the hit frame
 * 2. Character performs hit detection when OnHitWindowActive fires
 * 3. Damage/stun values come from the cached Action Command
 */
UCLASS()
class PROJECT_CANVAS_API UAnimNotify_CombatHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	/**
	 * Override of UAnimNotify::Notify
	 * Finds the CombatAnimationComponent and triggers hit window
	 * 
	 * @param MeshComp - The skeletal mesh playing the animation
	 * @param Animation - The animation sequence containing this notify
	 * @param EventReference - Event reference data
	 */
	virtual void Notify(USkeletalMeshComponent* MeshComp, 
					   UAnimSequenceBase* Animation, 
					   const FAnimNotifyEventReference& EventReference) override;
};