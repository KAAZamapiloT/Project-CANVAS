// Fill out your copyright notice in the Description page of Project Settings.

#include "SceneHistoryManager.h"
#include "Misc/DateTime.h"

// =============================================================================
// STATE MANAGEMENT
// =============================================================================

void USceneHistoryManager::SavePlan(const FEnhancedScenePlan& Plan, const FString& UserPrompt)
{
    UE_LOG(LogTemp, Display, TEXT("HistoryManager: SavePlan called for theme '%s'"), *Plan.ThemeName);
    
    // 1. Create delta record
    FSceneDelta Delta;
    Delta.Timestamp = FPlatformTime::Seconds();
    Delta.UserPrompt = UserPrompt;
    Delta.bModifiedEnvironment = Plan.bModifyEnvironment;
    
    // 2. Collect modified prop tags
    for (const FPropsModification& Prop : Plan.Props)
    {
        Delta.ModifiedTags.Add(Prop.TagName);
    }
    
    // 3. Determine change type (if we have previous state)
    if (CurrentState.Props.Num() > 0)
    {
        Delta.ChangeType = DetermineChangeType(CurrentState.Props[0], Plan.Props.Num() > 0 ? Plan.Props[0] : FPropsModification());
    }
    else
    {
        Delta.ChangeType = TEXT("Initial");
    }
    for (const FSpawnRequest& Spawn : Plan.SpawnRequest)
    {
        if (!Spawn.Tag.IsEmpty())
        {
            Delta.ModifiedTags.Add(Spawn.Tag);  // Track by unique Tag
        }
    }
    // 4. Update current state
    CurrentState = Plan;
    
    // 5. Add to history arrays
    PlanHistory.Add(Plan);
    PromptHistory.Add(UserPrompt);
    LogDelta(Delta);
    
    // 6. Trim if exceeds max size
    if (PlanHistory.Num() > MaxHistorySize)
    {
        PlanHistory.RemoveAt(0);
        PromptHistory.RemoveAt(0);
        DeltaLog.RemoveAt(0);
        UE_LOG(LogTemp, Warning, TEXT("HistoryManager: Trimmed history (exceeded max size %d)"), MaxHistorySize);
    }
    
    // 7. Update timestamps
    UpdateModificationTimestamps(Delta.ModifiedTags);
    
    UE_LOG(LogTemp, Display, TEXT("HistoryManager: Saved plan '%s' (History: %d plans)"), 
           *Plan.ThemeName, PlanHistory.Num());
}

bool USceneHistoryManager::RestorePlanAtIndex(int32 HistoryIndex)
{
    if (!PlanHistory.IsValidIndex(HistoryIndex))
    {
        UE_LOG(LogTemp, Error, TEXT("HistoryManager: Invalid history index %d (max: %d)"), 
               HistoryIndex, PlanHistory.Num() - 1);
        return false;
    }
    
    CurrentState = PlanHistory[HistoryIndex];
    UE_LOG(LogTemp, Warning, TEXT("HistoryManager: Restored plan from index %d - Theme: '%s'"), 
           HistoryIndex, *CurrentState.ThemeName);
    return true;
}

void USceneHistoryManager::ClearHistory()
{
    int32 OldCount = PlanHistory.Num();
    
    CurrentState = FEnhancedScenePlan();
    PlanHistory.Empty();
    PromptHistory.Empty();
    DeltaLog.Empty();
    LastModificationTimes.Empty();
    
    UE_LOG(LogTemp, Warning, TEXT("HistoryManager: Cleared %d history entries"), OldCount);
}

// =============================================================================
// DELTA & INTELLIGENCE
// =============================================================================

FEnhancedScenePlan USceneHistoryManager::ComputeDelta(const FEnhancedScenePlan& NewPlan)
{
    FEnhancedScenePlan Delta;
    Delta.ThemeName = NewPlan.ThemeName;
    
    UE_LOG(LogTemp, Display, TEXT("HistoryManager: Computing delta for '%s'"), *NewPlan.ThemeName);
    
    // === Check environment changes ===
    if (NewPlan.bModifyEnvironment)
    {
        bool bEnvChanged = false;
        
        // Compare fog density
        if (!FMath::IsNearlyEqual(NewPlan.Environment.FogDensity, CurrentState.Environment.FogDensity, 0.01f))
        {
            bEnvChanged = true;
            UE_LOG(LogTemp, Display, TEXT("  - FogDensity changed: %.2f -> %.2f"), 
                   CurrentState.Environment.FogDensity, NewPlan.Environment.FogDensity);
        }
        
        // Compare fog color
        if (NewPlan.Environment.FogColor != CurrentState.Environment.FogColor)
        {
            bEnvChanged = true;
            UE_LOG(LogTemp, Display, TEXT("  - FogColor changed"));
        }
        
        // Compare post-processing
        if (NewPlan.Environment.PostProcessingName != CurrentState.Environment.PostProcessingName)
        {
            bEnvChanged = true;
            UE_LOG(LogTemp, Display, TEXT("  - PostProcess changed: %s -> %s"), 
                   *CurrentState.Environment.PostProcessingName, *NewPlan.Environment.PostProcessingName);
        }
        
        if (bEnvChanged)
        {
            Delta.bModifyEnvironment = true;
            Delta.Environment = NewPlan.Environment;
        }
    }
    
    // === Check prop changes ===
    if (NewPlan.bModifyProps)
    {
        for (const FPropsModification& NewProp : NewPlan.Props)
        {
            bool bPropChanged = true;
            
            // Find matching prop in current state
            for (const FPropsModification& OldProp : CurrentState.Props)
            {
                if (OldProp.TagName == NewProp.TagName)
                {
                    // Use equality operator (if you added it to FPropsModification)
                    // For now, manual comparison:
                    bool bColorSame = (OldProp.PropColor == NewProp.PropColor);
                    bool bTextureSame = (OldProp.Texture.BaseColorPath == NewProp.Texture.BaseColorPath);
                    bool bParticleSame = (OldProp.ParticleEffects == NewProp.ParticleEffects);
                    
                    if (bColorSame && bTextureSame && bParticleSame)
                    {
                        bPropChanged = false;
                        UE_LOG(LogTemp, Display, TEXT("  - Prop '%s' unchanged"), *NewProp.TagName);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Display, TEXT("  - Prop '%s' changed (Color:%d Tex:%d FX:%d)"), 
                               *NewProp.TagName, !bColorSame, !bTextureSame, !bParticleSame);
                    }
                    break;
                }
            }
            
            if (bPropChanged)
            {
                Delta.Props.Add(NewProp);
                Delta.TargetPropTags.Add(NewProp.TagName);
            }
        }
        
        Delta.bModifyProps = (Delta.Props.Num() > 0);
    }
    
    if (NewPlan.bSpawnActors && NewPlan.SpawnRequest.Num() > 0)
    {
        Delta.bSpawnActors = true;
        // For spawns, we typically add all as new (unless implementing update/remove logic)
        Delta.SpawnRequest = NewPlan.SpawnRequest;
        UE_LOG(LogTemp, Display, TEXT(" - Spawn requests: %d"), NewPlan.SpawnRequest.Num());
    }
    
    UE_LOG(LogTemp, Display, TEXT("HistoryManager: Delta computed - ModifyEnv:%d, Props:%d, Spawns:%d"),
        Delta.bModifyEnvironment, Delta.Props.Num(), Delta.SpawnRequest.Num());
    
    return Delta;
}

FEnhancedScenePlan USceneHistoryManager::MergeWithCurrent(const FEnhancedScenePlan& PartialPlan)
{
    FEnhancedScenePlan Merged = CurrentState;
    Merged.ThemeName = PartialPlan.ThemeName; // Always update theme name
    
    UE_LOG(LogTemp, Display, TEXT("HistoryManager: Merging plan '%s' with current state"), 
           *PartialPlan.ThemeName);
    
    // Merge environment if requested
    if (PartialPlan.bModifyEnvironment)
    {
        Merged.Environment = PartialPlan.Environment;
        Merged.bModifyEnvironment = true;
        UE_LOG(LogTemp, Display, TEXT("  - Merged environment"));
    }
    
    // Merge props if requested
    if (PartialPlan.bModifyProps)
    {
        for (const FPropsModification& NewProp : PartialPlan.Props)
        {
            bool bFound = false;
            
            // Update existing prop
            for (FPropsModification& ExistingProp : Merged.Props)
            {
                if (ExistingProp.TagName == NewProp.TagName)
                {
                    ExistingProp = NewProp; // Replace
                    bFound = true;
                    UE_LOG(LogTemp, Display, TEXT("  - Updated prop '%s'"), *NewProp.TagName);
                    break;
                }
            }
            
            // Add new prop if not found
            if (!bFound)
            {
                Merged.Props.Add(NewProp);
                UE_LOG(LogTemp, Display, TEXT("  - Added new prop '%s'"), *NewProp.TagName);
            }
        }
        
        Merged.bModifyProps = true;
    }
    
    if (PartialPlan.bSpawnActors)
    {
        for (const FSpawnRequest& NewSpawn : PartialPlan.SpawnRequest)
        {
            bool bFound = false;
            // Check if spawn with same Tag already exists (for updates)
            for (FSpawnRequest& ExistingSpawn : Merged.SpawnRequest)
            {
                if (ExistingSpawn.Tag == NewSpawn.Tag && !NewSpawn.Tag.IsEmpty())
                {
                    ExistingSpawn = NewSpawn;  // Update existing
                    bFound = true;
                    UE_LOG(LogTemp, Display, TEXT(" - Updated spawn '%s'"), *NewSpawn.Tag);
                    break;
                }
            }
            // Add new spawn if not found
            if (!bFound)
            {
                Merged.SpawnRequest.Add(NewSpawn);
                UE_LOG(LogTemp, Display, TEXT(" - Added new spawn '%s'"), *NewSpawn.Tag);
            }
        }
        Merged.bSpawnActors = true;
    }
    
    UE_LOG(LogTemp, Display, TEXT("HistoryManager: Merge complete - Total props: %d, Spawns: %d"), 
        Merged.Props.Num(), Merged.SpawnRequest.Num());
    
    return Merged;
}

FString USceneHistoryManager::GetLLMContextString() const
{
    FString Context = TEXT("\n=== SCENE CONTEXT (for continuity) ===\n");
    
    // Add current theme
    if (!CurrentState.ThemeName.IsEmpty())
    {
        Context += FString::Printf(TEXT("Current Theme: '%s'\n"), *CurrentState.ThemeName);
    }
    
    // Add recent prompts (last 3)
    if (PromptHistory.Num() > 0)
    {
        Context += TEXT("Recent User Requests:\n");
        int32 StartIdx = FMath::Max(0, PromptHistory.Num() - 3);
        for (int32 i = StartIdx; i < PromptHistory.Num(); ++i)
        {
            Context += FString::Printf(TEXT("  %d. \"%s\"\n"), i + 1, *PromptHistory[i]);
        }
    }
    
    // Add current environment state
    Context += FString::Printf(TEXT("Current Fog: RGB[%d,%d,%d] Density:%.2f\n"),
        CurrentState.Environment.FogColor.R,
        CurrentState.Environment.FogColor.G,
        CurrentState.Environment.FogColor.B,
        CurrentState.Environment.FogDensity);
    
    // Add prop count
    Context += FString::Printf(TEXT("Modified Props: %d\n"), CurrentState.Props.Num());
    
    // Add total modifications
    Context += FString::Printf(TEXT("Total Modifications: %d\n"), DeltaLog.Num());
    
    Context += TEXT("=====================================\n");
    
    return Context;
}

bool USceneHistoryManager::HasConflict(const FEnhancedScenePlan& NewPlan, float ConflictWindowSeconds)
{
    float CurrentTime = FPlatformTime::Seconds();
    
    for (const FPropsModification& NewProp : NewPlan.Props)
    {
        const float* LastModTime = LastModificationTimes.Find(NewProp.TagName);
        if (LastModTime)
        {
            float TimeSinceLastMod = CurrentTime - *LastModTime;
            if (TimeSinceLastMod < ConflictWindowSeconds)
            {
                UE_LOG(LogTemp, Warning, TEXT("HistoryManager: CONFLICT - '%s' modified %.1fs ago (within %.1fs window)"),
                       *NewProp.TagName, TimeSinceLastMod, ConflictWindowSeconds);
                return true;
            }
        }
    }
    
    return false;
}

bool USceneHistoryManager::WasRecentlyModified(const FString& TagName, float WithinSeconds) const
{
    const float* LastTime = LastModificationTimes.Find(TagName);
    if (!LastTime)
    {
        return false;
    }
    
    float CurrentTime = FPlatformTime::Seconds();
    float TimeSince = CurrentTime - *LastTime;
    
    return (TimeSince <= WithinSeconds);
}

// =============================================================================
// ANALYTICS & DEBUGGING
// =============================================================================

TArray<FString> USceneHistoryManager::GetAllModifiedTags() const
{
    TArray<FString> AllTags;
    LastModificationTimes.GetKeys(AllTags);
    return AllTags;
}

FString USceneHistoryManager::GetRecentChangesSummary(int32 Count) const
{
    FString Summary = TEXT("=== Recent Changes ===\n");
    
    if (DeltaLog.Num() == 0)
    {
        Summary += TEXT("(No changes yet)\n");
        return Summary;
    }
    
    int32 StartIdx = FMath::Max(0, DeltaLog.Num() - Count);
    for (int32 i = StartIdx; i < DeltaLog.Num(); ++i)
    {
        const FSceneDelta& Delta = DeltaLog[i];
        Summary += FString::Printf(TEXT("%d. \"%s\" - %s (%d elements)\n"),
            i + 1, 
            *Delta.UserPrompt, 
            *Delta.ChangeType,
            Delta.ModifiedTags.Num());
    }
    
    return Summary;
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

void USceneHistoryManager::LogDelta(const FSceneDelta& Delta)
{
    DeltaLog.Add(Delta);
    
    UE_LOG(LogTemp, Display, TEXT("HistoryManager: Logged delta - Type:'%s', Tags:%d, Env:%d"),
           *Delta.ChangeType, Delta.ModifiedTags.Num(), Delta.bModifiedEnvironment);
}

void USceneHistoryManager::UpdateModificationTimestamps(const TArray<FString>& ModifiedTags)
{
    float CurrentTime = FPlatformTime::Seconds();
    
    for (const FString& Tag : ModifiedTags)
    {
        LastModificationTimes.Add(Tag, CurrentTime);
        UE_LOG(LogTemp, Verbose, TEXT("HistoryManager: Updated timestamp for '%s'"), *Tag);
    }
}

FString USceneHistoryManager::DetermineChangeType(const FPropsModification& OldProp, const FPropsModification& NewProp)
{
    // If NewProp is empty (initial state), return "Initial"
    if (NewProp.TagName.IsEmpty())
    {
        return TEXT("Initial");
    }
    
    bool bColorChanged = (OldProp.PropColor != NewProp.PropColor);
    bool bTextureChanged = (OldProp.Texture.BaseColorPath != NewProp.Texture.BaseColorPath ||
                           OldProp.Texture.NormalPath != NewProp.Texture.NormalPath);
    bool bParticleChanged = (OldProp.ParticleEffects != NewProp.ParticleEffects);
    
    // Determine type
    if (bColorChanged && bTextureChanged && bParticleChanged)
    {
        return TEXT("Full");
    }
    else if (bColorChanged && bTextureChanged)
    {
        return TEXT("ColorAndTexture");
    }
    else if (bColorChanged)
    {
        return TEXT("ColorOnly");
    }
    else if (bTextureChanged)
    {
        return TEXT("TextureOnly");
    }
    else if (bParticleChanged)
    {
        return TEXT("ParticleOnly");
    }
    
    return TEXT("Unknown");
}
