// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include"ScenePlan.h"
#include "SceneHistoryManager.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FSceneDelta
{
	GENERATED_BODY()
    
	UPROPERTY()
	float Timestamp;
    
	UPROPERTY()
	FString UserPrompt;
    
	UPROPERTY()
	TArray<FString> ModifiedTags;
    
	UPROPERTY()
	bool bModifiedEnvironment;
    
	UPROPERTY()
	FString ChangeType; // "ColorOnly", "TextureOnly", "Full"
};
UCLASS()
class PROJECT_CANVAS_API USceneHistoryManager : public UObject
{
	GENERATED_BODY()
public:
	// === STATE MANAGEMENT ===
    
    /** Save a new plan to history and update current state */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    void SavePlan(const FEnhancedScenePlan& Plan, const FString& UserPrompt);
    
    /** Get the current active scene state */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    FEnhancedScenePlan GetCurrentState() const { return CurrentState; }
    
    /** Restore a previous plan from history (for undo) */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    bool RestorePlanAtIndex(int32 HistoryIndex);
    
    /** Clear all history and reset to default state */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    void ClearHistory();
    
    // === DELTA & INTELLIGENCE ===
    
    /** Compute what changed between NewPlan and CurrentState */
    FEnhancedScenePlan ComputeDelta(const FEnhancedScenePlan& NewPlan);
    
    /** Merge a partial plan with CurrentState (for delta application) */
    FEnhancedScenePlan MergeWithCurrent(const FEnhancedScenePlan& PartialPlan);
    
    /** Generate context string for LLM (previous prompts + current state) */
    FString GetLLMContextString() const;
    
    /** Check if NewPlan conflicts with recent changes */
    bool HasConflict(const FEnhancedScenePlan& NewPlan, float ConflictWindowSeconds = 5.0f);
    
    /** Check if a specific tag was recently modified */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    bool WasRecentlyModified(const FString& TagName, float WithinSeconds = 10.0f) const;
    
    // === ANALYTICS & DEBUGGING ===
    
    /** Get total number of modifications made */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    int32 GetModificationCount() const { return DeltaLog.Num(); }
    
    /** Get all tags that have been modified at least once */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    TArray<FString> GetAllModifiedTags() const;
    
    /** Get summary of last N changes */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    FString GetRecentChangesSummary(int32 Count = 5) const;
    
    /** Check if we can undo (history exists) */
    UFUNCTION(BlueprintCallable, Category = "Scene History")
    bool CanUndo() const { return PlanHistory.Num() > 1; }

	
private:
	// === CURRENT STATE ===
	UPROPERTY()
	FEnhancedScenePlan CurrentState;
    
	// === HISTORY ===
	UPROPERTY()
	TArray<FEnhancedScenePlan> PlanHistory;
    
	UPROPERTY()
	TArray<FString> PromptHistory;
    
	UPROPERTY()
	TArray<FSceneDelta> DeltaLog;
    
	// === TRACKING ===
	UPROPERTY()
	TMap<FString, float> LastModificationTimes; // TagName -> Timestamp
    
	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 MaxHistorySize = 20; // Max plans to keep in memory
    
	// === HELPER FUNCTIONS ===
    
	/** Internal: Log a delta change */
	void LogDelta(const FSceneDelta& Delta);
    
	/** Internal: Update modification timestamps for changed tags */
	void UpdateModificationTimestamps(const TArray<FString>& ModifiedTags);
    
	/** Internal: Detect what type of change occurred */
	FString DetermineChangeType(const FPropsModification& OldProp, const FPropsModification& NewProp);
};
