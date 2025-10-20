// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h"
#include "SceneBuilder.generated.h"

/**
 * SceneBuilder - Applies FEnhancedScenePlan to the world
 * Handles environment settings, prop modifications, and texture loading
 */
UCLASS()
class PROJECT_CANVAS_API USceneBuilder : public UObject
{
	GENERATED_BODY()
    
public:
	UFUNCTION(BlueprintCallable, Category = "Scene Builder")
	void BuildScene(const struct FEnhancedScenePlan& Plan, UWorld* WorldContext);

private:
	// Main build functions
	void ApplyEnvironmentSettings(const FEnvironmentPlan& Environment, UWorld* WorldContext);
	void ApplyPropsModifications(const TArray<FPropsModification>& Props, UWorld* WorldContext);
    
	// Environment helpers
	void ApplyFogSettings(float FogDensity, FColor FogColor, UWorld* WorldContext);
	void ApplyPostProcessing(const FString& PostProcessingName, UWorld* WorldContext);
    
	// Props helpers
	void ModifyPropsWithTag(const FPropsModification& PropMod, UWorld* WorldContext);
	void ApplyTextureSetToMesh(UStaticMeshComponent* Mesh, const FTextureSet& TextureSet);
	void ApplyParticleEffects(AActor* Actor, const FString& ParticleEffectsName);
    
	// Texture loading helpers
	UTexture2D* LoadTextureFromPath(const FString& TexturePath);
	UMaterialInstanceDynamic* GetOrCreateDynamicMaterial(UStaticMeshComponent* Mesh, int32 MaterialIndex = 0);
};