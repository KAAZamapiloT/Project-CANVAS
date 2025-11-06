// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ScenePlan.h"
#include "JsonParser.generated.h"

/**
 * JSON Parser - Converts LLM JSON output into Enhanced Scene Plans
 * 
 * Responsibilities:
 * 1. Validate JSON schema
 * 2. Parse JSON into FEnhancedScenePlan struct
 * 3. Extract all plan components (environment, props, spawn requests)
 * 
 * NOTE: Location resolution is NOT done here!
 * That's SceneStateTracker's responsibility in the enrichment pipeline.
 */
UCLASS()
class PROJECT_CANVAS_API UJsonParser : public UObject
{
    GENERATED_BODY()
    
public:
    /**
     * Validate JSON schema before parsing
     * 
     * @param JsonContext - Raw JSON string to validate
     * @return true if schema is valid, false otherwise
     */
    UFUNCTION(BlueprintCallable)
    static bool bScehmaValidation(FString& JsonContext);
    
    /**
     * Parse JSON string into FEnhancedScenePlan
     * 
     * This is the MAIN parser that converts LLM JSON output
     * into structured plan data. It handles:
     * - Theme name extraction
     * - Environment settings (lighting, fog, post-process)
     * - Prop modifications (colors, textures, effects)
     * - Spawn requests (mesh, location, rotation, scale)
     * 
     * @param JsonContext - Raw JSON string from LLM
     * @return Parsed FEnhancedScenePlan (may be incomplete if JSON is invalid)
     */
    UFUNCTION(BlueprintCallable)
    static FEnhancedScenePlan CreatePlan(FString JsonContext);

private:
    /**
     * Parse environment settings from JSON object
     * 
     * @param JsonObject - JSON object containing environment data
     * @param Environment - Output structure to populate
     */
    static void ParseEnvironment(const TSharedPtr<FJsonObject>& JsonObject, FEnvironmentPlan& Environment);
    
    /**
     * Parse prop modification from JSON object
     * 
     * @param JsonObject - JSON object containing prop data
     * @param PropMod - Output structure to populate
     */
    static void ParsePropModification(const TSharedPtr<FJsonObject>& JsonObject, FPropsModification& PropMod);
    
    /**
     * Parse texture set from JSON object
     * 
     * @param JsonObject - JSON object containing texture paths
     * @param TextureSet - Output structure to populate
     */
    static void ParseTextureSet(const TSharedPtr<FJsonObject>& JsonObject, FTextureSet& TextureSet);
    
    /**
     * Parse spawn request from JSON object
     * 
     * Extracts:
     * - AssetPath and ObjectName (semantic identifiers)
     * - LocationName (semantic location like "PLAYER_FRONT")
     * - Rotation, Scale, Offset
     * - ClearanceRadius
     * 
     * NOTE: SpawnLocation is NOT resolved here - that happens in SceneStateTracker
     * 
     * @param JsonObject - JSON object containing spawn data
     * @param SpawnRequest - Output structure to populate
     */
    static void ParseSpawnRequest(const TSharedPtr<FJsonObject>& JsonObject, FSpawnRequest& SpawnRequest);
};
