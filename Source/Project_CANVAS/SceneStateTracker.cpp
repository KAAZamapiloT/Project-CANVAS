// Fill out your copyright notice in the Description page of Project Settings.


// SceneStateTracker.cpp

#include "SceneStateTracker.h"
#include "GenAISystem.h"      
#include "SceneBuilder.h"
#include "SceneHistoryManager.h"



void USceneStateTracker::Init()
{
	Super::Init();
    
	UE_LOG(LogTemp, Warning, TEXT("===== SceneStateTracker::Init() CALLED! ====="));
	// 1. Create AssetIndexer
	AssetIndexer = NewObject<UAssetIndexer>(this);
	if (!AssetIndexer)
	{
		UE_LOG(LogTemp, Error, TEXT("SceneStateTracker: FAILED to create AssetIndexer!"));
		return;
	}
    
	// 2. Create GenAISystem
	GenAISystem = NewObject<UGenAISystem>(this);
	if (!GenAISystem)
	{
		UE_LOG(LogTemp, Error, TEXT("SceneStateTracker: FAILED to create GenAISystem!"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("SceneStateTracker: GenAISystem created successfully!"));
    
	// 3. Create SceneBuilder
	SceneBuilder = NewObject<USceneBuilder>(this);
	if (!SceneBuilder)
	{
		UE_LOG(LogTemp, Error, TEXT("SceneStateTracker: FAILED to create SceneBuilder!"));
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("SceneStateTracker: SceneBuilder created successfully!"));
	// 4 . Create HistoryManger
	HistoryManager = NewObject<USceneHistoryManager>(this);
	if (!HistoryManager)
	{
		UE_LOG(LogTemp, Error, TEXT("SceneStateTracker: FAILED to create HistoryManager!"));
		return;
	}
	UE_LOG(LogTemp,Warning, TEXT("SceneStateTracker: SceneHistoryManager created successfully!"));
	
	
    
	// Bind to completion delegate
	AssetIndexer->OnScanComplete.AddDynamic(this, &USceneStateTracker::OnAssetScanFinished);
    GenAISystem->OnThemeDataReady.AddDynamic(this, &USceneStateTracker::OnPlanReceived);
	// Start comprehensive scan - pass WorldContext when available
	// Note: World might not be ready in Init(), so you might need to delay this
	if (GetWorld())
	{
		AssetIndexer->ScanAllAssetsAsync(GetWorld());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SceneStateTracker: World not ready, scanning assets only (no tags)"));
		// Fallback: scan just assets, not level tags
		AssetIndexer->ScanForTexturesAsync(TEXT("/Game/DATABASE/textures"));
		AssetIndexer->ScanForParticlesAsync(TEXT("/Game/DATABASE/particles"));
		AssetIndexer->ScanForPostProcessMaterialsAsync(TEXT("/Game/DATABASE/postprocess"));
	}
}

void USceneStateTracker::OnAssetScanFinished()
{
	UE_LOG(LogTemp, Warning, TEXT("GAME INSTANCE: Asset scan complete! All data ready."));
    
	// Optionally log what we found
	TArray<FString> Textures = AssetIndexer->GetDiscoveredTextureNames();
	TArray<FString> Particles = AssetIndexer->GetDiscoveredParticleNames();
	TArray<FString> Tags = AssetIndexer->GetDiscoveredActorTags();
    
	UE_LOG(LogTemp, Display, TEXT("Available Textures: %s"), *FString::Join(Textures, TEXT(", ")));
	UE_LOG(LogTemp, Display, TEXT("Available Particles: %s"), *FString::Join(Particles, TEXT(", ")));
	UE_LOG(LogTemp, Display, TEXT("Available Tags: %s"), *FString::Join(Tags, TEXT(", ")));
}

void USceneStateTracker::OnPlanReceived(const FEnhancedScenePlan& Plan, const FString& UserPrompt)
{
	UE_LOG(LogTemp, Log, TEXT("SceneStateTracker: Received plan for rehydration."));
	FEnhancedScenePlan EnrichedPlan = Plan;

	ResolveTexturesFromNames(EnrichedPlan); // ensures names map to texture paths

	if (HistoryManager)
		HistoryManager->SavePlan(EnrichedPlan, UserPrompt);

	if (SceneBuilder && GetWorld())
		SceneBuilder->BuildScene(EnrichedPlan, GetWorld());
}


void USceneStateTracker::ResolveTexturesFromNames(FEnhancedScenePlan& Plan)
{
	if (!AssetIndexer)
	{
		UE_LOG(LogTemp, Error, TEXT("SceneStateTracker: AssetIndexer not initialized."));
		return;
	}

	for (FPropsModification& Prop : Plan.Props)
	{
		FString TextureKey = Prop.Texture.BaseColorPath;
		if (TextureKey.IsEmpty())
			continue;

		// Prefer fully qualified material name resolution
		FTextureSet Set = AssetIndexer->ResolveTextureFromName(TextureKey);

		// Fallback: use hybrid reconstruction if direct lookup failed
		if (Set.BaseColorPath.IsEmpty())
			Set = AssetIndexer->ResolveBaseMaterialToTextureSet(TextureKey);

		if (Set.BaseColorPath.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("SceneStateTracker: Could not resolve any textures for %s"), *TextureKey);
			continue;
		}

		Prop.Texture = Set;
		UE_LOG(LogTemp, Display, TEXT("Resolved Prop '%s': Diff=%s, Norm=%s, Rough=%s, Metal=%s, AO=%s"),
			*Prop.TagName,
			*Set.BaseColorPath, *Set.NormalPath, *Set.RoughnessPath, *Set.MetallicPath, *Set.AOPath);
	}
}

