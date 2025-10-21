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

void USceneStateTracker::OnPlanReceived(const FEnhancedScenePlan& Plan,const FString& UserPrompt)
{
	UE_LOG(LogTemp, Warning, TEXT("SceneStateTracker: Plan Received!"));
	FEnhancedScenePlan EnrichedPlan = Plan;

    if (!AssetIndexer)
    {
        UE_LOG(LogTemp, Error, TEXT("SceneStateTracker: AssetIndexer is null! Cannot resolve materials."));
    }
    else
    {
        for (FPropsModification& Prop : EnrichedPlan.Props)
        {
            FString& BaseName = Prop.Texture.BaseColorPath;

            const bool bLooksLikeBase =
                !BaseName.IsEmpty() &&
                !BaseName.Contains(TEXT("_diff_")) &&
                !BaseName.Contains(TEXT("_rough_")) &&
                !BaseName.Contains(TEXT("_nor_")) &&
                !BaseName.Contains(TEXT("_metal_")) &&
                !BaseName.Contains(TEXT("_arm_")) &&
                !BaseName.Contains(TEXT("_ao_"));

            if (bLooksLikeBase)
            {
                const FTextureSet Resolved = AssetIndexer->ResolveBaseMaterialToTextureSet(BaseName);
                if (!Resolved.BaseColorPath.IsEmpty())
                {
                    UE_LOG(LogTemp, Display, TEXT("Resolved tag '%s': %s -> Diff:%s Rough:%s Normal:%s Metal:%s AO:%s"),
                        *Prop.TagName, *BaseName,
                        *Resolved.BaseColorPath, *Resolved.RoughnessPath,
                        *Resolved.NormalPath, *Resolved.MetallicPath, *Resolved.AOPath);
                    Prop.Texture = Resolved;
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("SceneStateTracker: Failed to resolve base '%s' for tag '%s'"),
                        *BaseName, *Prop.TagName);
                }
            }
        }

        // Debug: print all props after enrichment
        for (const FPropsModification& Prop : EnrichedPlan.Props)
        {
            UE_LOG(LogTemp, Display, TEXT("Resolved Prop '%s' -> Diff:%s Normal:%s Rough:%s Metal:%s AO:%s"),
                *Prop.TagName,
                *Prop.Texture.BaseColorPath,
                *Prop.Texture.NormalPath,
                *Prop.Texture.RoughnessPath,
                *Prop.Texture.MetallicPath,
                *Prop.Texture.AOPath);
        }
    }

    if (HistoryManager)
    {
        HistoryManager->SavePlan(EnrichedPlan, UserPrompt);
        UE_LOG(LogTemp, Display, TEXT("SceneStateTracker: Saved plan to history"));
    }

    if (SceneBuilder && GetWorld())
    {
        SceneBuilder->BuildScene(EnrichedPlan, GetWorld());
    }
}
