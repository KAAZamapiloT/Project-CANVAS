// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Common/TypedElementCommonTypes.h"
#include "GameFramework/Actor.h"
#include "ScenePlan.generated.h"

/**
 * FEnhancedScenePlan -> Data structure that defines how the scene will be built
 * Think of it as ingredients and recipe
 * 
 * Output from JSON Parser → Input to Scene Builder
 */

// LIGHTING 
USTRUCT(BlueprintType)
struct FLightingPlan
{
    GENERATED_BODY()
    
public:
    // Directional Light (Sun/Moon)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SunColor = FLinearColor::White;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SunIntensity = 10.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SunPitch = -45.0f;  // Angle: -90 (overhead) to 0 (horizon)
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SunYaw = 0.0f;  // Direction: 0 (north), 90 (east), 180 (south), 270 (west)
    
    // Sky Light (Ambient)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor SkyLightColor = FLinearColor(0.2f, 0.3f, 0.5f);  // Soft blue
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SkyLightIntensity = 1.0f;
    
    // Temperature (Kelvin) - optional color warmth
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SunTemperature = 6500.0f;  // 6500 = daylight, 3000 = warm sunset, 9000 = cool blue
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseTemperature = false;  // Use color OR temperature


    bool operator==(const FLightingPlan& Other) const
    {
        return SunColor.Equals(Other.SunColor, 0.01f) &&
               FMath::IsNearlyEqual(SunIntensity, Other.SunIntensity, 0.1f) &&
               FMath::IsNearlyEqual(SunPitch, Other.SunPitch, 0.1f) &&
               FMath::IsNearlyEqual(SunYaw, Other.SunYaw, 0.1f) &&
               SkyLightColor.Equals(Other.SkyLightColor, 0.01f) &&
               FMath::IsNearlyEqual(SkyLightIntensity, Other.SkyLightIntensity, 0.1f) &&
               FMath::IsNearlyEqual(SunTemperature, Other.SunTemperature, 0.1f) &&
               bUseTemperature == Other.bUseTemperature;
    }
};



// === SPAWNING (RESERVED FOR FUTURE) ===
USTRUCT(BlueprintType)
struct FSpawnRequest
{
    GENERATED_BODY()
    
    /** Asset path for mesh (e.g., "SM_Rock") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AssetPath;
    
    /** Name of the object (e.g., "Rock") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ObjectName;
    
    /** Location Of Spawn*/
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector SpawnLocation;
    
    /** 🆕 SEMANTIC LOCATION STRING (e.g., "PLAYER_FRONT", "Arena_Center") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LocationName;
    
    /** Optional: Offset from resolved location (e.g., +50 units up) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector LocationOffset = FVector::ZeroVector;
    
    /** Rotation for the spawned actor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Rotation = FRotator::ZeroRotator;
    
    /** Scale for the spawned actor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Scale = FVector::OneVector;
    
    /** Actor tag for tracking (e.g., "GenAI.Spawned.Rock") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Tag;
    
    /** 🆕 Minimum clearance radius required (default: 150 units) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ClearanceRadius = 150.0f;
};


// === PBR TEXTURE SET ===
/**
 * A PBR texture set. All paths are FStrings from the Asset Indexer.
 * The SceneBuilder will asynchronously load these.
 */
USTRUCT(BlueprintType)
struct FTextureSet
{
    GENERATED_BODY()
    
    // Core maps
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BaseColorPath;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NormalPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RoughnessPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MetallicPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AOPath;

    // Optional extras
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DisplacementPath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OpacityPath;
    
    // === EQUALITY OPERATOR FOR DELTA COMPARISON ===
    bool operator==(const FTextureSet& Other) const
    {
        return BaseColorPath == Other.BaseColorPath &&
               NormalPath == Other.NormalPath &&
               RoughnessPath == Other.RoughnessPath &&
               MetallicPath == Other.MetallicPath &&
               AOPath == Other.AOPath;
    }
    
    bool operator!=(const FTextureSet& Other) const
    {
        return !(*this == Other);
    }
};

// === ENVIRONMENT SETTINGS ===
/**
 * Defines global environmental settings (fog, post-processing)
 */
USTRUCT(BlueprintType)
struct FEnvironmentPlan
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLightingPlan Lighting;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FogDensity = 0.1f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FColor FogColor = FColor::White;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PostProcessingName;
    
    // === EQUALITY OPERATOR FOR DELTA COMPARISON ===
    bool operator==(const FEnvironmentPlan& Other) const
    {
        return FMath::IsNearlyEqual(FogDensity, Other.FogDensity, 0.01f) &&
               FogColor == Other.FogColor &&
               PostProcessingName == Other.PostProcessingName;
    }
    
    bool operator!=(const FEnvironmentPlan& Other) const
    {
        return !(*this == Other);
    }
};

// === PROP MODIFICATION ===
/**
 * Defines all modifications for a single prop or group of props
 */
USTRUCT(BlueprintType)
struct FPropsModification
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TagName;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FColor PropColor = FColor::White;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTextureSet Texture;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ParticleEffects;
    
    // === EQUALITY OPERATOR FOR DELTA COMPARISON ===
    bool operator==(const FPropsModification& Other) const
    {
        return TagName == Other.TagName &&
               PropColor == Other.PropColor &&
               Texture == Other.Texture &&
               ParticleEffects == Other.ParticleEffects;
    }
    
    bool operator!=(const FPropsModification& Other) const
    {
        return !(*this == Other);
    }
};

// === MAIN SCENE PLAN ===
/**
 * Complete scene modification plan
 * Includes modification flags for delta/incremental changes
 */
USTRUCT(BlueprintType)
struct FEnhancedScenePlan
{
    GENERATED_BODY()

    // === METADATA ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ThemeName;

    // === DELTA FLAGS (NEW) ===
    /** If true, apply Environment changes. If false, skip environment. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bModifyEnvironment = false;
    
    /** If true, apply Props changes. If false, skip props. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bModifyProps = false;
    
    /** OPTIONAL: List of specific prop tags to modify. Empty = modify all in Props array. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> TargetPropTags;
    
    // === SCENE DATA ===
    /** Environmental changes (fog, post-process) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FEnvironmentPlan Environment;
    
    /** List of all props to change */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FPropsModification> Props;
    
    // === DEPRECATED FLAGS (Keep for backward compatibility) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DeprecatedProperty, DeprecationMessage = "Use bModifyEnvironment instead"))
    bool bChangeEnviroment = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DeprecatedProperty, DeprecationMessage = "Use bModifyProps instead"))
    bool bChangeProps = false;

    // Planing which actors to spawn
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSpawnRequest> SpawnRequest;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSpawnRequest> ParticleSpawns;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSpawnActors=false;
    
    // === FUTURE EXTENSIONS (Commented out for now) ===
    // UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // TArray<FSpawnRequest> NewSpawns;
    //UPROPERTY(EditAnywhere, BlueprintReadWrite)
    // FStruct post process; 
    // fstruct dynamic objects loading

    
    // === EQUALITY OPERATOR FOR DELTA COMPARISON ===
    bool operator==(const FEnhancedScenePlan& Other) const
    {
        return ThemeName == Other.ThemeName &&
               Environment == Other.Environment &&
               Props.Num() == Other.Props.Num();
    }
    
    bool operator!=(const FEnhancedScenePlan& Other) const
    {
        return !(*this == Other);
    }
};
