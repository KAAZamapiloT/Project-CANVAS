// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AssetIndexer.h"
#include "LocationQueryEngine.h"
#include "SceneStateTracker.generated.h"

// Forward declarations
class UGenAISystem;
class USceneBuilder;
class USceneHistoryManager;

/**
 * Core game instance that manages all AI-driven scene generation subsystems.
 *
 * This class orchestrates the AssetIndexer, LocationQueryEngine, GenAISystem,
 * and SceneBuilder. It also serves as the central manager for tracking all
 * actors spawned by the AI.
 */
UCLASS()
class PROJECT_CANVAS_API USceneStateTracker : public UGameInstance
{
	GENERATED_BODY()

public:
	// ========================================================================
	// SUBSYSTEMS - Core Components
	// ========================================================================

	/** Discovers and maintains the database of all available game assets. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
	UAssetIndexer* AssetIndexer;

	/** Scans the level for named spawn points and resolves spatial queries. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
	ULocationQueryEngine* LocationEngine;

	/** Manages LLM API connections, prompt generation, and scene plan parsing. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
	UGenAISystem* GenAISystem;

	/** Executes scene plans by spawning actors and applying materials. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
	USceneBuilder* SceneBuilder;

	/** Logs all scene modifications to enable undo/redo functionality. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
	USceneHistoryManager* HistoryManager;

	// ========================================================================
	// CONFIGURATION - Editable Settings
	// ========================================================================

	/** Actor tags that can be targeted by scene generation requests (e.g., "Floor", "Wall"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Generation|Configuration")
	TArray<FString> TargetableActorTags;

	/** List of available post-process materials for scene effects (e.g., "PP_Cyberpunk"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Generation|Configuration")
	TArray<FString> AvailablePostProcessMaterials;

	// ========================================================================
	// ACTOR STORAGE - Spawned Actors Management
	// ========================================================================

	/** Master list of all actors spawned by the SceneBuilder. */
	UPROPERTY(VisibleAnywhere, Category = "Scene Generation|Actor Storage")
	TArray<AActor*> SpawnedActors;

	/** Fast lookup map for retrieving a spawned actor by its unique name (Name -> Actor). */
	UPROPERTY(VisibleAnywhere, Category = "Scene Generation|Actor Storage")
	TMap<FString, AActor*> SpawnedActorsByName;

	/** Reverse lookup map for finding an actor's unique name from its pointer (Actor -> Name). */
	UPROPERTY(VisibleAnywhere, Category = "Scene Generation|Actor Storage")
	TMap<AActor*, FString> ActorToNameMap;

	// ========================================================================
	// CORE INITIALIZATION
	// ========================================================================

	/**
	 * Initializes all subsystems and starts asynchronous asset/location scans.
	 * Called automatically when the game instance is created.
	 */
	virtual void Init() override;

	// ========================================================================
	// CALLBACKS - Async Completion Handlers
	// ========================================================================

	/** Callback triggered by AssetIndexer when its async scan is complete. */
	UFUNCTION()
	void OnAssetScanFinished();

	/** Callback triggered by LocationQueryEngine when its async scan is complete. */
	UFUNCTION()
	void OnLocationScanFinished();

	/**
	 * Main callback to receive and execute a scene plan from the GenAISystem.
	 * This triggers the plan enrichment and scene build phases.
	 *
	 * @param Plan          The scene plan received from the GenAI system.
	 * @param UserPrompt    The original user prompt, for history tracking.
	 */
	UFUNCTION()
	void OnPlanReceived(const FEnhancedScenePlan& Plan, const FString& UserPrompt);

	// ========================================================================
	// ENRICHMENT FUNCTIONS - Resolve Semantic Names to Asset Paths
	// ========================================================================

	/**
	 * Resolves semantic texture names in the plan (e.g., "Concrete") to full asset paths.
	 * @param Plan The scene plan, which will be modified in place.
	 */
	UFUNCTION()
	void ResolveTexturesFromNames(FEnhancedScenePlan& Plan);

	/**
	 * Resolves semantic mesh names in the plan (e.g., "Chair") to full asset paths.
	 * @param Plan The scene plan, which will be modified in place.
	 */
	UFUNCTION()
	void ResolveMeshesFromNames(FEnhancedScenePlan& Plan);

	/**
	 * Resolves semantic location names in the plan (e.g., "PLAYER_FRONT") to world coordinates.
	 * @param Plan The scene plan, which will be modified in place.
	 */
	UFUNCTION()
	void ResolveLocationsInPlan(FEnhancedScenePlan& Plan);

	// ========================================================================
	// ACTOR MANAGEMENT - Query & Removal Functions
	// ========================================================================

	/** Prints a verbose debug report of all currently spawned actors to the log. */
	UFUNCTION(BlueprintCallable, Category = "Scene Generation|Debug")
	void PrintAllSpawnedActors() const;

	/**
	 * Gets a spawned actor by its unique assigned name (O(1) lookup).
	 * @param ObjectName The unique name (e.g., "Chair_01").
	 * @return AActor pointer or nullptr if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scene Generation|Debug")
	AActor* GetSpawnedActorByName(const FString& ObjectName) const;

	/**
	 * Gets the count of currently spawned actors.
	 * @return int32 count of spawned actors.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scene Generation|Debug")
	int32 GetSpawnedActorCount() const { return SpawnedActors.Num(); }

	/**
	 * Gets the unique name of a spawned actor from its pointer (reverse lookup).
	 * @param Actor Pointer to the actor.
	 * @return The actor's unique name, or "Unknown" if not found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scene Generation|Debug")
	FString GetActorNameByActor(AActor* Actor) const;

	/**
	 * Removes and destroys a spawned actor by its unique name. This is the primary removal method.
	 * @param ObjectName Unique name (e.g., "Chair_01").
	 * @return true if removed, false if failed (e.g., not found).
	 */
	UFUNCTION(BlueprintCallable, Category = "Scene Generation|Actor Management")
	bool RemoveSpawnedActorByName(const FString& ObjectName);

	/**
	 * Removes and destroys a spawned actor using its actor pointer.
	 * @param Actor Actor pointer to remove.
	 * @return true if removed, false if failed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scene Generation|Actor Management")
	bool RemoveSpawnedActorByActor(AActor* Actor);

	/**
	 * Removes and destroys multiple actors in a batch from an array of names.
	 * @param ObjectNames Array of names to remove.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scene Generation|Actor Management")
	void RemoveMultipleActors(const TArray<FString>& ObjectNames);

	/**
	 * Clears and destroys ALL spawned actors.
	 * Use for complete scene resets or level transitions.
	 */
	UFUNCTION(BlueprintCallable, Category = "Scene Generation|Actor Management")
	void ClearAllSpawnedActors();

	// ========================================================================
	// INTERNAL FUNCTIONS - Private Implementation
	// ========================================================================
	/**
	 * Callback when SceneBuilder spawns a new actor
	 *
	 * Called during execution phase for each spawned actor
	 * Automatically stores actor in SpawnedActors storage
	 *
	 * @param NewActor - Newly spawned actor
	 * @param ObjectName - Unique name to assign to actor
	 */
	UFUNCTION()
	void OnActorSpawned(AActor* NewActor, const FString& ObjectName);
private:
	/**
	 * Initialization state tracking
	 * Used to determine when all async subsystems are ready
	 */
	bool bAssetScanComplete = false;
	bool bLocationScanComplete = false;

	/**
	 * Actor naming counter for dynamic unique names
	 * Incremented each time an actor is assigned a name
	 * Ensures no two actors share the same name
	 */
	int32 ActorNameCounter = 0;

	/**
	 * Check if all critical systems are initialized and ready
	 *
	 * Called when both asset and location scans complete
	 * Logs readiness status and enables GenAI requests
	 *
	 * Conditions for readiness:
	 * ✓ bAssetScanComplete == true
	 * ✓ bLocationScanComplete == true
	 * ✓ AssetIndexer != nullptr
	 * ✓ LocationEngine != nullptr
	 * ✓ GenAISystem != nullptr
	 * ✓ SceneBuilder != nullptr
	 *
	 * Output when ready:
	 * ╔════════════════════════════╗
	 * ║ ✅ ALL SYSTEMS READY!    ║
	 * ║ GenAI requests enabled   ║
	 * ╚════════════════════════════╝
	 */
	void CheckSystemsReady();



	/**
	 * Log current scene state to output log
	 *
	 * Called periodically for debugging
	 * Shows count of spawned actors and storage usage
	 */
	void LogSceneStateVerbose();
	
private:
	// 1. Store the plan here so it persists while waiting for HTTP
	FEnhancedScenePlan PendingPlan;

	// 2. The "Funnel" function. This builds the scene.
	// It takes optional results from the LLM.
	// Change parameter type
	void FinalizeSceneBuild(const TMap<FString, FResolutionResult>& AsyncResults);
	
	FTimerHandle InitTimerHandle;
	
	void DelayedInit();
public:
	/**
 * Visualizes the playable area bounds with debug geometry.
 * @param Duration How long to display (seconds).
 */
	UFUNCTION(Exec, BlueprintCallable, Category = "LocationEngine|Debug")
	void VisualizeBounds(float Duration=10.f);

	void OnStart() override;;

};