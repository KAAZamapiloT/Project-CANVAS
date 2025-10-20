// Fill out your copyright notice in the Description page of Project Settings.

// SceneBuilder.cpp

#include "SceneBuilder.h"
#include "ScenePlan.h"
#include "Engine/World.h"
#include "UObject/SoftObjectPath.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleSystem.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h" // <-- Needed for GetComponent()
#include "Engine/PostProcessVolume.h"

// --- Define your content paths ---
// You MUST place your assets in these folders, or change these paths.
// The parser gets "T_Brick_Normal", this path turns it into "/Game/Textures/Generative/T_Brick_Normal.T_Brick_Normal"
const FString GTextureBasePath = TEXT("/Game/DATABASE/textures/");
const FString GParticleBasePath = TEXT("/Game/DATABASE/particles/");
const FString GPostProcessBasePath = TEXT("/Game/DATABASE/postprocess/");


// --- Main Public Function ---

void USceneBuilder::BuildScene(const FEnhancedScenePlan& Plan, UWorld* WorldContext)
{
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Error, TEXT("SceneBuilder: WorldContext is null. Cannot build scene."));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Building scene for theme: %s"), *Plan.ThemeName);

    // Apply all global environmental settings
    ApplyEnvironmentSettings(Plan.Environment, WorldContext);

    // Apply all specific prop/actor modifications
    ApplyPropsModifications(Plan.Props, WorldContext);
}

// --- Private: Main Build Functions ---

void USceneBuilder::ApplyEnvironmentSettings(const FEnvironmentPlan& Environment, UWorld* WorldContext)
{
    // 1. Apply Fog
    ApplyFogSettings(Environment.FogDensity, Environment.FogColor, WorldContext);

    // 2. Apply Post Processing
    // This assumes "PostProcessingName" is the name of an asset, e.g., "PP_Cyberpunk"
    ApplyPostProcessing(Environment.PostProcessingName, WorldContext);
    
    // NOTE: Your ScenePlan.h doesn't have lights yet.
    // When you add them, you would find the ADirectionalLight and apply
    // Environment.PrimaryLight properties here.
}

void USceneBuilder::ApplyPropsModifications(const TArray<FPropsModification>& Props, UWorld* WorldContext)
{
    // Loop through every modification request in the plan
    for (const FPropsModification& PropMod : Props)
    {
        if (PropMod.TagName.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Skipping PropMod with empty ActorTag."));
            continue;
        }
        ModifyPropsWithTag(PropMod, WorldContext);
    }
}

// --- Private: Environment Helpers ---

void USceneBuilder::ApplyFogSettings(float FogDensity, FColor FogColor, UWorld* WorldContext)
{
    // Find the single Exponential Height Fog actor in the level
    AExponentialHeightFog* FogActor = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(WorldContext, AExponentialHeightFog::StaticClass()));
    
    if (FogActor)
    {
        UE_LOG(LogTemp, Log, TEXT("SceneBuilder: Applying Fog Settings."));
        UExponentialHeightFogComponent* FogComponent = FogActor->GetComponent();
        if (FogComponent)
        {
            FogComponent->SetFogDensity(FogDensity);
            FogComponent->SetFogInscatteringColor(FogColor.ReinterpretAsLinear());
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: No AExponentialHeightFog actor found in level."));
    }
}

void USceneBuilder::ApplyPostProcessing(const FString& PostProcessingName, UWorld* WorldContext)
{
    if (PostProcessingName.IsEmpty()) return;

    // Find the main Post Process Volume in the level
    TArray<AActor*> PPVolumes;
    UGameplayStatics::GetAllActorsOfClass(WorldContext, APostProcessVolume::StaticClass(), PPVolumes);

    if (PPVolumes.Num() > 0)
    {
        APostProcessVolume* PPVolume = Cast<APostProcessVolume>(PPVolumes[0]);
        if (PPVolume)
        {
            // Construct the full asset path
            // e.g., "MI_Cyberpunk" -> "/Game/DATABASE/postprocess/MI_Cyberpunk.MI_Cyberpunk"
            FString FullPath = FString::Printf(TEXT("%s%s.%s"), *GPostProcessBasePath, *PostProcessingName, *PostProcessingName);
            FSoftObjectPath AssetPath = FSoftObjectPath(FullPath);

            // Get the Streamable Manager for async loading
            FStreamableManager& Streamer = UAssetManager::Get().GetStreamableManager();
            
            // Use a weak pointer for the callback to be safe
            TWeakObjectPtr<APostProcessVolume> WeakPPVolume = PPVolume;

            // Request the load. This runs on a background thread.
            Streamer.RequestAsyncLoad(AssetPath, [WeakPPVolume, AssetPath]()
            {
                // This lambda function runs on the GAME THREAD after loading is complete
                if (!WeakPPVolume.IsValid()) return; // Volume was destroyed

                UMaterialInterface* LoadedMaterial = Cast<UMaterialInterface>(AssetPath.ResolveObject());
                if (LoadedMaterial)
                {
                    UE_LOG(LogTemp, Log, TEXT("SceneBuilder: Applying Post Process Material: %s"), *LoadedMaterial->GetName());
                    
                    // --- FIX: Access the .Array member ---
                    // Clear any old materials
                    WeakPPVolume->Settings.WeightedBlendables.Array.Empty();

                    // Create the new material entry
                    FWeightedBlendable NewBlendable;
                    NewBlendable.Object = LoadedMaterial;
                    NewBlendable.Weight = 1.0f; // Fully apply it

                    // Add it to the volume's settings
                    WeakPPVolume->Settings.WeightedBlendables.Array.Add(NewBlendable);
                }
                else
                {
                    UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Failed to load Post Process Material: %s"), *AssetPath.ToString());
                }
            });
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: No APostProcessVolume actor found in level."));
    }
}


// --- Private: Props Helpers ---

void USceneBuilder::ModifyPropsWithTag(const FPropsModification& PropMod, UWorld* WorldContext)
{
    // Find all actors with the specified tag (e.g., "Background.Wall")
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(WorldContext, FName(*PropMod.TagName), FoundActors);

    if (FoundActors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Found 0 actors with tag: %s"), *PropMod.TagName);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("SceneBuilder: Modifying %d actors with tag: %s"), FoundActors.Num(), *PropMod.TagName);

    for (AActor* Actor : FoundActors)
    {
        UStaticMeshComponent* Mesh = Actor->FindComponentByClass<UStaticMeshComponent>();
        if (Mesh)
        {
            // Apply the new set of PBR textures
            ApplyTextureSetToMesh(Mesh, PropMod.Texture);

            // Apply the simple color tint
            UMaterialInstanceDynamic* MID = GetOrCreateDynamicMaterial(Mesh, 0);
            if (MID)
            {
                // This assumes your material has a Vector Parameter named "BaseColorTint"
                MID->SetVectorParameterValue(TEXT("BaseColorTint"), PropMod.PropColor);
            }
        }
        
        // Apply particle effects
        ApplyParticleEffects(Actor, PropMod.ParticleEffects);
    }
}

void USceneBuilder::ApplyTextureSetToMesh(UStaticMeshComponent* Mesh, const FTextureSet& TextureSet)
{
    if (!Mesh) return;

    // Get the Streamable Manager for async loading
    FStreamableManager& Streamer = UAssetManager::Get().GetStreamableManager();
    
    // Create a map of Material Parameter Names -> Texture Paths
    TMap<FName, FString> TextureParams;
    TextureParams.Add(TEXT("BaseTexture"), TextureSet.BaseColorPath); // Assumes MID param is "BaseTexture"
    TextureParams.Add(TEXT("NormalMap"), TextureSet.NormalPath);     // Assumes MID param is "NormalMap"
    TextureParams.Add(TEXT("RoughnessMap"), TextureSet.RoughnessPath);
    TextureParams.Add(TEXT("MetallicMap"), TextureSet.MetallicPath);
    TextureParams.Add(TEXT("AOMap"), TextureSet.AOPath);

    for (const TPair<FName, FString>& Param : TextureParams)
    {
        if (Param.Value.IsEmpty()) continue; // Skip if no texture was specified

        // Construct the full asset path from the name
        // e.g., "T_Brick_N" -> "/Game/Textures/Generative/T_Brick_N.T_Brick_N"
        FString FullPath = FString::Printf(TEXT("%s%s.%s"), *GTextureBasePath, *Param.Value, *Param.Value);
        FSoftObjectPath AssetPath = FSoftObjectPath(FullPath);
        
        TWeakObjectPtr<UStaticMeshComponent> WeakMeshPtr = Mesh;
        FName ParamName = Param.Key;
        TWeakObjectPtr<USceneBuilder> WeakThis = this;
        
        // --- FIX 1: Changed [this, ...] to [WeakThis, ...] ---
        // Request the load. This runs on a background thread.
        Streamer.RequestAsyncLoad(AssetPath, [WeakThis, WeakMeshPtr, ParamName, AssetPath]()
        {
            // This lambda function runs on the GAME THREAD after loading is complete
            // --- FIX 1: Single validity check for both weak pointers ---
            if (!WeakThis.IsValid() || !WeakMeshPtr.IsValid()) return;
            
            UTexture2D* LoadedTexture = Cast<UTexture2D>(AssetPath.ResolveObject());
            if (LoadedTexture)
            {
                // Get or create the dynamic material
                // Now this line is safe and correct
                UMaterialInstanceDynamic* MID = WeakThis->GetOrCreateDynamicMaterial(WeakMeshPtr.Get());
                
                if (MID)
                {
                    // This is thread-safe and applies the texture!
                    UE_LOG(LogTemp, Log, TEXT("SceneBuilder: Applying texture %s to param %s"), *LoadedTexture->GetName(), *ParamName.ToString());
                    MID->SetTextureParameterValue(ParamName, LoadedTexture);
                }
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Failed to load texture: %s"), *AssetPath.ToString());
            }
        });
    }
}

void USceneBuilder::ApplyParticleEffects(AActor* Actor, const FString& ParticleEffectName)
{
    if (!Actor || ParticleEffectName.IsEmpty()) return;

    // Find an existing component to update
    UParticleSystemComponent* ParticleComp = Actor->FindComponentByClass<UParticleSystemComponent>();
    if (!ParticleComp)
    {
        // If one doesn't exist, create it
        ParticleComp = NewObject<UParticleSystemComponent>(Actor);
        ParticleComp->RegisterComponent();
        ParticleComp->AttachToComponent(Actor->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
    }

    // Construct the asset path
    FString FullPath = FString::Printf(TEXT("%s%s.%s"), *GParticleBasePath, *ParticleEffectName, *ParticleEffectName);
    FSoftObjectPath AssetPath = FSoftObjectPath(FullPath);

    TWeakObjectPtr<UParticleSystemComponent> WeakParticleComp = ParticleComp;

    // --- FIX 2: Changed GetStreamStreamer() to GetStreamableManager() ---
    // Async load the particle system
    FStreamableManager& Streamer = UAssetManager::Get().GetStreamableManager();
    Streamer.RequestAsyncLoad(AssetPath, [WeakParticleComp, AssetPath]()
    {
        // This runs on the Game Thread when loading is done
        if (!WeakParticleComp.IsValid()) return;
        
        UParticleSystem* LoadedSystem = Cast<UParticleSystem>(AssetPath.ResolveObject());
        if (LoadedSystem)
        {
            UE_LOG(LogTemp, Log, TEXT("SceneBuilder: Applying particle system %s"), *LoadedSystem->GetName());
            WeakParticleComp->SetTemplate(LoadedSystem);
            WeakParticleComp->ActivateSystem();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Failed to load particle system: %s"), *AssetPath.ToString());
        }
    });
}

// --- Private: Texture/Material Helpers ---

UMaterialInstanceDynamic* USceneBuilder::GetOrCreateDynamicMaterial(UStaticMeshComponent* Mesh, int32 MaterialIndex)
{
    if (!Mesh) return nullptr;

    // Get the material from the slot
    UMaterialInterface* Material = Mesh->GetMaterial(MaterialIndex);
    if (!Material)
    {
        UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Mesh has no material at index %d"), MaterialIndex);
        return nullptr;
    }

    // Check if it's already a MID
    UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Material);
    if (MID)
    {
        return MID; // It's already dynamic, just return it
    }
    
    // It's a static material, so create a new MID from it
    MID = Mesh->CreateDynamicMaterialInstance(MaterialIndex, Material);
    if (MID)
    {
        UE_LOG(LogTemp, Log, TEXT("SceneBuilder: Created new MID for mesh %s"), *Mesh->GetName());
        return MID;
    }
    
    return nullptr;
}

UTexture2D* USceneBuilder::LoadTextureFromPath(const FString& TexturePath)
{
    // ---
    // WARNING: This is a SYNCHRONOUS (blocking) load and will cause
    // your game to HITCH or FREEZE.
    // It is not used by the new async code.
    // ---
    if (TexturePath.IsEmpty()) return nullptr;
    
    UE_LOG(LogTemp, Warning, TEXT("SceneBuilder: Using SLOW Synchronous load for %s. This will cause a hitch!"), *TexturePath);
    return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *TexturePath));
}
