// Fill out your copyright notice in the Description page of Project Settings.


// AnimNotify_CombatHit.cpp

#include "AnimNotify_CombatHit.h"
#include "CombatAnimationComponent.h"

void UAnimNotify_CombatHit::Notify(USkeletalMeshComponent* MeshComp, 
								   UAnimSequenceBase* Animation, 
								   const FAnimNotifyEventReference& EventReference)
{
	// Call parent implementation
	Super::Notify(MeshComp, Animation, EventReference);

	// Get the actor that owns this skeletal mesh
	if (AActor* Owner = MeshComp->GetOwner())
	{
		// Find the CombatAnimationComponent on the owner
		if (UCombatAnimationComponent* CombatComp = Owner->FindComponentByClass<UCombatAnimationComponent>())
		{
			// Trigger the hit window callback
			// This broadcasts OnHitWindowActive with cached damage/stun
			CombatComp->HandleHitNotify();
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AnimNotify_CombatHit: Owner has no CombatAnimationComponent"));
		}
	}
}
