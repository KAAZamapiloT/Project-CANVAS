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
 * ============================================================================
 * SCENE STATE TRACKER - Central Orchestrator for AI-Driven Scene Generation
 * ============================================================================
 * 
 * USceneStateTracker is the core game instance that manages all scene
 * generation systems for AI-driven procedural level creation.
 * 
 * ARCHITECTURE:
 * ============================================================================
 * 
 * This class coordinates five main subsystems:
 * 
 * 1. AssetIndexer
 *    - Discovers all available assets in the game
 *    - Scans textures, meshes, particles, and actor tags
 *    - Maintains searchable databases for LLM prompts
 *    - Runs async to avoid frame rate stalls
 * 
 * 2. LocationQueryEngine
 *    - Scans the level for named spawn points
 *    - Maintains mapping of semantic names to world coordinates
 *    - Examples: "PLAYER_FRONT" → FVector(500, 200, 0)
 *    - Provides collision checking and clearance validation
 * 
 * 3. GenAISystem
 *    - Connects to LLM APIs (Groq, OpenAI, etc.)
 *    - Constructs prompts from available assets
 *    - Parses JSON responses into scene plans
 *    - Emits delegates when plans are ready
 * 
 * 4. SceneBuilder
 *    - Executes scene plans produced by GenAI
 *    - Spawns new actors with resolved paths
 *    - Applies materials, textures, and lighting
 *    - Manages actor lifecycle and storage
 * 
 * 5. HistoryManager
 *    - Logs all scene modifications
 *    - Enables undo/redo functionality
 *    - Provides scene replay capabilities
 * 
 * LIFECYCLE:
 * ============================================================================
 * 
 * Phase 1: INITIALIZATION (Init)
 *   - Create all subsystems
 *   - Bind delegates for async callbacks
 *   - Start async asset and location scans
 *   - Set initialization flags to false
 * 
 * Phase 2: SCANNING (Async)
 *   - AssetIndexer scans /Game/DATABASE for assets
 *   - LocationEngine queries level actors for spawn points
 *   - Both complete independently and emit callbacks
 * 
 * Phase 3: READINESS CHECK
 *   - OnAssetScanFinished() and OnLocationScanFinished() called
 *   - Both must complete before GenAI requests are processed
 *   - CheckSystemsReady() logs when all systems are ready
 * 
 * Phase 4: GENERATION (On User Request)
 *   - Player enters prompt or GenAI trigger fires
 *   - GenAI creates prompt from asset database
 *   - Calls LLM API and gets JSON scene plan
 *   - Emits OnPlanReceived delegate
 * 
 * Phase 5: ENRICHMENT (OnPlanReceived)
 *   - Resolve texture names → full asset paths
 *   - Resolve mesh names → full asset paths
 *   - Resolve semantic locations → world coordinates
 *   - Validate all paths exist and are accessible
 * 
 * Phase 6: EXECUTION (SceneBuilder)
 *   - Spawn actors with resolved mesh paths
 *   - Apply textures and materials
 *   - Apply lighting modifications
 *   - Register actors in SpawnedActors storage
 * 
 * Phase 7: TRACKING
 *   - SceneBuilder notifies OnActorSpawned
 *   - Actors stored by dynamic unique names
 *   - History logged for undo/redo
 * 
 * DATA STORAGE:
 * ============================================================================
 * 
 * SpawnedActors (TArray)
 *   - Sequential list of all spawned actors
 *   - Used for iteration and bulk operations
 *   - Example: [Actor1, Actor2, Actor3]
 * 
 * SpawnedActorsByName (TMap)
 *   - Name → Actor pointer mapping
 *   - Fast lookup by unique object name
 *   - Example: {"Chair_01" → Actor*, "Table_02" → Actor*}
 *   - Guarantees O(1) lookup time
 * 
 * ActorToNameMap (TMap)
 *   - Reverse mapping: Actor → Name
 *   - Used for name lookup from actor pointer
 *   - Enables bidirectional queries
 * 
 * ERROR HANDLING:
 * ============================================================================
 * 
 * - All functions log verbose status messages
 * - Failed operations return false/nullptr
 * - Null checks on all actor operations
 * - Memory is properly deallocated on removal
 * - TArray and TMap use Reset() to clear (not Clear())
 * 
 * EXAMPLE USAGE:
 * ============================================================================
 * 
 * // Get the tracker
 * USceneStateTracker* Tracker = Cast<USceneStateTracker>(GetGameInstance());
 * 
 * // Query spawned actors
 * AActor* Chair = Tracker->GetSpawnedActorByName(TEXT("Chair_01"));
 * int32 Count = Tracker->GetSpawnedActorCount();
 * 
 * // Remove specific actor
 * Tracker->RemoveSpawnedActorByName(TEXT("Chair_01"));
 * 
 * // Remove multiple
 * TArray<FString> ToRemove = {"Chair_01", "Table_02"};
 * Tracker->RemoveMultipleActors(ToRemove);
 * 
 * // Debug output
 * Tracker->PrintAllSpawnedActors();
 * 
 * // Clear everything
 * Tracker->ClearAllSpawnedActors();
 * 
 * ============================================================================
 */
UCLASS()
class PROJECT_CANVAS_API USceneStateTracker : public UGameInstance
{
    GENERATED_BODY()

public:
    // ========================================================================
    // SUBSYSTEMS - Core Components
    // ========================================================================

    /**
     * Asset Indexer - Discovers and maintains asset database
     * 
     * Responsible for:
     * - Scanning /Game/DATABASE/ for textures, meshes, particles
     * - Building searchable indices for LLM prompts
     * - Tagging assets with metadata
     * - Running async scans without blocking gameplay
     * 
     * Emits: OnAssetScanFinished() delegate when complete
     * Status: Check IsScanComplete() before using
     * 
     * Example Assets Discovered:
     *   Textures: [T_Concrete, T_Wood, T_Brick, ...]
     *   Meshes: [SM_Chair, SM_Table, SM_Rock, ...]
     *   Particles: [P_Fire, P_Smoke, ...]
     *   Tags: ["Floor", "Wall", "Ceiling", ...]
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
    UAssetIndexer* AssetIndexer;

    /**
     * Location Query Engine - Spatial data for spawning
     * 
     * Responsible for:
     * - Scanning level for named actors (spawn points)
     * - Building semantic name → world coordinate mappings
     * - Checking collision/clearance for safe spawning
     * - Resolving relative positions (PLAYER_FRONT, PLAYER_LEFT, etc.)
     * 
     * Emits: OnLocationScanFinished() delegate when complete
     * Status: Check IsScanComplete() before using
     * 
     * Example Locations:
     *   "PLAYER_FRONT" → FVector(500, 200, 100)
     *   "SPAWN_AREA_1" → FVector(1000, 500, 0)
     *   "BOSS_ARENA_CENTER" → FVector(2000, 2000, 500)
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
    ULocationQueryEngine* LocationEngine;

    /**
     * GenAI System - LLM integration for scene generation
     * 
     * Responsible for:
     * - Constructing natural language prompts from assets
     * - Calling LLM API (Groq, OpenAI, etc.)
     * - Parsing JSON responses into FEnhancedScenePlan
     * - Emitting OnPlanReceived delegate
     * 
     * Flow:
     *   User Input → GenAI::RequestSceneChange()
     *   → Constructs Prompt with available assets
     *   → HTTP POST to LLM
     *   → JSON Response Parsed
     *   → OnPlanReceived() Broadcast
     * 
     * API Keys: Stored in config file (DefaultGame.ini) for security
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
    UGenAISystem* GenAISystem;

    /**
     * Scene Builder - Executes scene plans
     * 
     * Responsible for:
     * - Spawning actors with resolved asset paths
     * - Applying materials and textures
     * - Modifying lighting and environmental effects
     * - Notifying SceneStateTracker of spawned actors
     * 
     * Flow:
     *   OnPlanReceived() → Enrichment Phase
     *   → Execution Phase (SceneBuilder::BuildScene)
     *   → Spawning Phase (SpawnNewActors)
     *   → OnActorSpawned() callbacks
     * 
     * Sync vs Async: Mesh loading is SYNCHRONOUS for predictability
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
    USceneBuilder* SceneBuilder;

    /**
     * History Manager - Tracks scene modifications
     * 
     * Responsible for:
     * - Logging all scene changes
     * - Enabling undo/redo functionality
     * - Saving/loading scene states
     * - Providing scene replay capabilities
     * 
     * Data Stored:
     *   - Theme name and user prompt
     *   - Spawned actors and locations
     *   - Applied materials and textures
     *   - Timestamp of execution
     * 
     * Example History Entry:
     *   { Theme: "dark_theme", Actors: 3, Materials: 5, Time: 12:34:56 }
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Scene Generation|Subsystems")
    USceneHistoryManager* HistoryManager;

    // ========================================================================
    // CONFIGURATION - Editable Settings
    // ========================================================================

    /**
     * Actor Tags Available for Targeting
     * 
     * These tags identify groups of actors in the level that can be
     * modified by scene generation requests.
     * 
     * Examples: ["Floor", "Wall", "Ceiling", "Prop", "Light"]
     * 
     * Usage:
     *   - GenAI prompt includes these tags
     *   - Scene plans target specific tags
     *   - SceneBuilder applies modifications to tagged actors
     * 
     * Edit in: Project Settings → Scene Generation → Targetable Tags
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Generation|Configuration")
    TArray<FString> TargetableActorTags;

    /**
     * Post-Process Materials Available for Scene Effects
     * 
     * Post-process materials stored in /Game/DATABASE/postprocess/
     * that can be applied for scene atmosphere and mood.
     * 
     * Examples: ["PP_Cyberpunk", "PP_Horror", "PP_Fantasy", "PP_Vintage"]
     * 
     * Usage:
     *   - GenAI prompt includes available PPMs
     *   - Scene plans specify which PPM to apply
     *   - SceneBuilder applies selected PPM
     * 
     * Edit in: Project Settings → Scene Generation → Available PPMs
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene Generation|Configuration")
    TArray<FString> AvailablePostProcessMaterials;

    // ========================================================================
    // ACTOR STORAGE - Spawned Actors Management
    // ========================================================================

    /**
     * Array of all spawned actors
     * 
     * Storage: TArray<AActor*>
     * Use: Iteration, bulk operations, reference counting
     * 
     * Example Contents:
     *   [0] → Chair actor spawned at (100, 200, 50)
     *   [1] → Table actor spawned at (300, 200, 0)
     *   [2] → Lamp actor spawned at (100, 200, 200)
     * 
     * IMPORTANT: Always check pointer validity before use
     *   if (SpawnedActors[i] && !SpawnedActors[i]->IsActorBeingDestroyed())
     * 
     * See Also: SpawnedActorsByName (faster lookup), ActorToNameMap
     */
    UPROPERTY(VisibleAnywhere, Category = "Scene Generation|Actor Storage")
    TArray<AActor*> SpawnedActors;

    /**
     * Map of actor names to actor pointers - FAST LOOKUP
     * 
     * Storage: TMap<FString ObjectName, AActor*>
     * Use: O(1) lookup by object name, remove by name
     * 
     * Example Contents:
     *   "Chair_01" → Actor1*
     *   "Table_02" → Actor2*
     *   "Lamp_GenAI_03" → Actor3*
     * 
     * Naming Convention:
     *   - Generated dynamically by GenerateUniqueName()
     *   - Format: "GenAI_BaseName_Counter"
     *   - Guaranteed unique (no duplicates)
     * 
     * Operations:
     *   - Add:    SpawnedActorsByName.Add("Chair_01", ActorPtr)
     *   - Remove: SpawnedActorsByName.Remove("Chair_01")
     *   - Find:   SpawnedActorsByName.Contains("Chair_01")
     *   - Get:    AActor* = SpawnedActorsByName["Chair_01"]
     * 
     * See Also: SpawnedActors (array), ActorToNameMap (reverse lookup)
     */
    UPROPERTY(VisibleAnywhere, Category = "Scene Generation|Actor Storage")
    TMap<FString, AActor*> SpawnedActorsByName;

    /**
     * Reverse map: Actor pointer to name
     * 
     * Storage: TMap<AActor*, FString ObjectName>
     * Use: Lookup actor name from pointer
     * 
     * Example Contents:
     *   Actor1* → "Chair_01"
     *   Actor2* → "Table_02"
     *   Actor3* → "Lamp_GenAI_03"
     * 
     * Use Case: When you have an actor and need to find its name
     *   FString ActorName = ActorToNameMap[MyActor];
     *   Tracker->RemoveSpawnedActorByName(ActorName);
     * 
     * See Also: SpawnedActorsByName (name to actor mapping)
     */
    UPROPERTY(VisibleAnywhere, Category = "Scene Generation|Actor Storage")
    TMap<AActor*, FString> ActorToNameMap;

    // ========================================================================
    // CORE INITIALIZATION
    // ========================================================================

    /**
     * Initialize all subsystems and start async scans
     * 
     * Called automatically when game instance is created (before gameplay starts)
     * 
     * Actions Performed:
     * 1. Create AssetIndexer
     *    - Scans /Game/DATABASE/ for assets (async)
     *    - Emits OnAssetScanFinished() when complete
     * 
     * 2. Create LocationQueryEngine
     *    - Scans level for spawn points (async)
     *    - Emits OnLocationScanFinished() when complete
     * 
     * 3. Create GenAISystem
     *    - Sets up LLM connection
     *    - Loads API keys from config
     * 
     * 4. Create SceneBuilder
     *    - Initializes scene execution system
     *    - Binds actor spawn callbacks
     * 
     * 5. Create HistoryManager
     *    - Sets up history tracking
     * 
     * Flow:
     *   Init() Called
     *   ├─ Create Subsystems
     *   ├─ Bind Delegates
     *   ├─ Start Async Scans
     *   ├─ Set Flags (bAssetScanComplete=false, bLocationScanComplete=false)
     *   └─ Return (scans continue in background)
     * 
     * @note: Systems are not ready for GenAI requests until both scans complete
     * @see CheckSystemsReady(), OnAssetScanFinished(), OnLocationScanFinished()
     */
    virtual void Init() override;

    // ========================================================================
    // CALLBACKS - Async Completion Handlers
    // ========================================================================

    /**
     * Called when AssetIndexer completes asset scanning
     * 
     * Triggered By: AssetIndexer::OnScanFinished() delegate
     * 
     * Actions:
     * 1. Log discovered assets (count of textures, meshes, etc.)
     * 2. Set bAssetScanComplete = true
     * 3. Call CheckSystemsReady() to check if all systems initialized
     * 
     * Output Example:
     *   ✅ Asset Scan Complete!
     *   📊 Materials: 46
     *   🎨 Textures: 286
     *   🏛️  Static Meshes: 321
     *   🏷️  Actor Tags: 3
     * 
     * @note: Must complete before GenAI requests are accepted
     * @see CheckSystemsReady()
     */
    UFUNCTION()
    void OnAssetScanFinished();

    /**
     * Called when LocationQueryEngine completes location scanning
     * 
     * Triggered By: LocationQueryEngine::OnScanFinished() delegate
     * 
     * Actions:
     * 1. Log discovered spawn locations (named points in level)
     * 2. Set bLocationScanComplete = true
     * 3. Call CheckSystemsReady() to check if all systems initialized
     * 
     * Output Example:
     *   ✅ Location Scan Complete!
     *   📍 Spawn Points: 12
     *   🎯 Named Locations: PLAYER_FRONT, PLAYER_LEFT, PLAYER_BACK
     * 
     * @note: Must complete before GenAI requests are accepted
     * @see CheckSystemsReady()
     */
    UFUNCTION()
    void OnLocationScanFinished();

    /**
     * Main callback: Receives and executes scene plans from GenAI
     * 
     * Triggered By: GenAISystem::OnThemeDataReady delegate
     * Binding: SetupInputComponent() in player controller
     * 
     * Parameters:
     * @param Plan - FEnhancedScenePlan containing scene modifications
     *               (textures, materials, spawn requests, etc.)
     * @param UserPrompt - Original user input for history tracking
     * 
     * Execution Flow:
     * 1. Validate world and subsystems
     * 2. ENRICHMENT PHASE:
     *    - ResolveTexturesFromNames() - Convert "Wood" → full path
     *    - ResolveMeshesFromNames() - Convert "Chair" → full path
     *    - ResolveLocationsInPlan() - Convert "PLAYER_FRONT" → FVector
     * 3. EXECUTION PHASE:
     *    - SceneBuilder::BuildScene() - Spawn and apply
     * 4. HISTORY PHASE:
     *    - HistoryManager::LogSceneExecution() - Track changes
     * 5. LOGGING:
     *    - LogSceneStateVerbose() - Status update
     * 
     * Output Example:
     *   ╔═══════════════════════╗
     *   ║ 📥 Plan Received      ║
     *   ║ Theme: dark_theme     ║
     *   ║ Actors: 3             ║
     *   ║ Materials: 5          ║
     *   ╚═══════════════════════╝
     *   ✅ Enrichment Complete
     *   ✅ Execution Complete
     *   ✅ Plan Applied!
     * 
     * @note: All phase failures log errors but don't crash
     * @see ResolveTexturesFromNames(), ResolveMeshesFromNames(),
     *      ResolveLocationsInPlan(), SceneBuilder::BuildScene()
     */
    UFUNCTION()
    void OnPlanReceived(const FEnhancedScenePlan& Plan, const FString& UserPrompt);

    // ========================================================================
    // ENRICHMENT FUNCTIONS - Resolve Semantic Names to Asset Paths
    // ========================================================================

    /**
     * Resolve texture semantic names to full asset paths
     * 
     * Purpose: Convert LLM-friendly names to UE4 asset paths
     * 
     * Example Conversions:
     *   Input:  "Concrete" (from GenAI)
     *   Output: "/Game/DATABASE/textures/Concrete/T_Concrete_BaseColor.T_Concrete_BaseColor"
     * 
     *   Input:  "Wood"
     *   Output: "/Game/DATABASE/textures/Wood/T_Wood_BC.T_Wood_BC"
     * 
     * Process:
     * 1. Loop through all props in scene plan
     * 2. For each texture name, query AssetIndexer
     * 3. Find exact or fuzzy match in asset database
     * 4. Replace semantic name with full path
     * 5. Log success/failure
     * 
     * Validation:
     *   - Checks texture exists in database
     *   - Verifies file is loadable
     *   - Logs warnings for missing textures
     * 
     * @param Plan - Scene plan (modified in place - replaces names with paths)
     * 
     * @note: Runs synchronously during enrichment phase
     * @see OnPlanReceived()
     */
    UFUNCTION()
    void ResolveTexturesFromNames(FEnhancedScenePlan& Plan);

    /**
     * Resolve mesh semantic names to full asset paths
     * 
     * Purpose: Convert LLM-friendly mesh names to UE4 mesh paths
     * 
     * Example Conversions:
     *   Input:  "Chair" (from GenAI)
     *   Output: "/Game/DATABASE/meshes/Furniture/SM_Chair.SM_Chair"
     * 
     *   Input:  "Rock"
     *   Output: "/Game/DATABASE/meshes/Natural/SM_Rock.SM_Rock"
     * 
     * Process:
     * 1. Loop through all spawn requests in scene plan
     * 2. For each mesh name, query AssetIndexer
     * 3. Find exact match in asset database (must be exact - no fuzzy)
     * 4. Replace semantic name with full asset path
     * 5. Log success/failure
     * 
     * Validation:
     *   - Exact match required (no partial names)
     *   - Verifies mesh is valid StaticMesh type
     *   - Logs errors for missing meshes
     * 
     * @param Plan - Scene plan (modified in place - replaces names with paths)
     * 
     * @note: Runs synchronously during enrichment phase
     * @see OnPlanReceived()
     */
    UFUNCTION()
    void ResolveMeshesFromNames(FEnhancedScenePlan& Plan);

    /**
     * Resolve semantic location names to world coordinates
     * 
     * Purpose: Convert human-readable spawn locations to 3D coordinates
     * 
     * Example Conversions:
     *   Input:  "PLAYER_FRONT" (from GenAI)
     *   Output: FVector(1412.9, 380.3, 87.2)
     * 
     *   Input:  "SPAWN_AREA_1"
     *   Output: FVector(2000, 500, 100)
     * 
     * Process:
     * 1. Loop through all spawn requests in scene plan
     * 2. For each location name, query LocationQueryEngine
     * 3. Engine returns world coordinate + clearance info
     * 4. Replace semantic location name with resolved FVector
     * 5. Apply offset if specified in spawn request
     * 6. Log success/failure
     * 
     * Features:
     *   - Collision checking to ensure safe spawn
     *   - Clearance radius validation
     *   - Offset application for fine-tuning
     *   - Fallback to alternative locations if blocked
     * 
     * @param Plan - Scene plan (modified in place - replaces names with coords)
     * 
     * @note: Runs synchronously during enrichment phase
     * @note: Validates world collision data
     * @see LocationQueryEngine::ResolveLocation()
     * @see OnPlanReceived()
     */
    UFUNCTION()
    void ResolveLocationsInPlan(FEnhancedScenePlan& Plan);

    // ========================================================================
    // ACTOR MANAGEMENT - Query & Removal Functions
    // ========================================================================

    /**
     * Query: Print all spawned actors to log (verbose debug output)
     * 
     * Purpose: See complete list of spawned actors with details
     * 
     * Output Format:
     *   ╔══════════════════════════════════╗
     *   ║ 📋 Spawned Actors Report (3)   ║
     *   ╠══════════════════════════════════╣
     *   ║ [1] Chair_01 at (100, 200, 50) ║
     *   ║     Actor: StaticMeshActor_0    ║
     *   ║     Mesh: SM_Chair              ║
     *   ║ [2] Table_02 at (300, 200, 0)  ║
     *   ║     Actor: StaticMeshActor_1    ║
     *   ║     Mesh: SM_Table              ║
     *   ║ [3] Lamp_GenAI_03 at (100, ...) ║
     *   ║     Actor: StaticMeshActor_2    ║
     *   ║     Mesh: SM_Lamp               ║
     *   ╚══════════════════════════════════╝
     * 
     * @note: Callable from Blueprint via Utility Widget
     * @note: Shows actor name, type, and location
     */
    UFUNCTION(BlueprintCallable, Category = "Scene Generation|Debug")
    void PrintAllSpawnedActors() const;

    /**
     * Query: Get spawned actor by unique name
     * 
     * Purpose: Fast lookup of actor by its assigned name
     * 
     * Example Usage:
     *   AActor* Chair = Tracker->GetSpawnedActorByName(TEXT("Chair_01"));
     *   if (Chair)
     *   {
     *       Chair->SetActorHiddenInGame(true);  // Hide it
     *   }
     * 
     * Performance: O(1) - HashMap lookup, very fast
     * 
     * Returns:
     *   - Valid AActor* if found
     *   - nullptr if not found (logs warning)
     * 
     * @param ObjectName - Unique name assigned during spawn
     *                     Example: "Chair_01", "Lamp_GenAI_03"
     * 
     * @return AActor* pointer or nullptr if not found
     * 
     * @note: Safe to call - null check on return before use
     */
    UFUNCTION(BlueprintCallable, Category = "Scene Generation|Debug")
    AActor* GetSpawnedActorByName(const FString& ObjectName) const;

    /**
     * Query: Get count of currently spawned actors
     * 
     * Purpose: Quick check of how many actors are spawned
     * 
     * Returns: Number of actors in SpawnedActors array
     * 
     * Example Usage:
     *   int32 Count = Tracker->GetSpawnedActorCount();
     *   UE_LOG(LogTemp, Warning, TEXT("Spawned: %d actors"), Count);
     *
     * @return int32 count of spawned actors (0 if none)
     */
    UFUNCTION(BlueprintCallable, Category = "Scene Generation|Debug")
    int32 GetSpawnedActorCount() const { return SpawnedActors.Num(); }

    /**
     * Query: Get actor name by actor pointer (reverse lookup)
     * 
     * Purpose: Find the unique name assigned to an actor
     * 
     * Example Usage:
     *   FString ActorName = Tracker->GetActorNameByActor(SomeActor);
     *   if (!ActorName.IsEmpty())
     *   {
     *       Tracker->RemoveSpawnedActorByName(ActorName);
     *   }
     * 
     * @param Actor - Pointer to actor to find name for
     * @return Unique name assigned to actor, or "Unknown" if not found
     * 
     * @note: Uses ActorToNameMap for reverse lookup
     */
    UFUNCTION(BlueprintCallable, Category = "Scene Generation|Debug")
    FString GetActorNameByActor(AActor* Actor) const;

    /**
     * Remove spawned actor by its unique name - PRIMARY REMOVAL METHOD
     * 
     * Purpose: Delete a specific spawned actor from the world
     * 
     * Example Usage:
     *   bool bRemoved = Tracker->RemoveSpawnedActorByName(TEXT("Chair_01"));
     *   if (bRemoved)
     *   {
     *       UE_LOG(LogTemp, Warning, TEXT("Chair removed!"));
     *   }
     * 
     * Actions Performed:
     * 1. Validate name exists in SpawnedActorsByName
     * 2. Get actor pointer from map
     * 3. Validate actor pointer is valid
     * 4. Check actor is not already being destroyed
     * 5. Remove from SpawnedActorsByName (name → actor)
     * 6. Remove from ActorToNameMap (actor → name)
     * 7. Remove from SpawnedActors array
     * 8. Call Actor->Destroy() (queues destruction)
     * 9. Log detailed information about removal
     * 
     * Returns:
     *   - true if successfully removed and destroyed
     *   - false if actor not found or error occurred
     * 
     * Error Handling:
     *   - Name not found: Log warning, return false
     *   - Null pointer: Log error, return false
     *   - Already destroying: Log warning, return false
     * 
     * Output Example:
     *   ╔════════════════════════════════════╗
     *   ║ ✅ ACTOR REMOVED SUCCESSFULLY    ║
     *   ╚════════════════════════════════════╝
     *   ObjectName: Chair_01
     *   ActorLabel: Actor_Chair_01
     *   Position: [1412.9, 380.3, 87.2]
     *   Remaining: 2 actors
     * 
     * @param ObjectName - Unique name (example: "Chair_01")
     * @return true if removed, false if failed
     * 
     * @note: Name must be exact match (case-sensitive)
     * @note: Actor is queued for destruction (not immediate)
     * @note: Guaranteed O(1) lookup via TMap
     * @see RemoveSpawnedActorByActor(), RemoveMultipleActors()
     */
    UFUNCTION(BlueprintCallable, Category = "Scene Generation|Actor Management")
    bool RemoveSpawnedActorByName(const FString& ObjectName);

    /**
     * Remove spawned actor by actor pointer
     * 
     * Purpose: Delete actor when you have the pointer (not the name)
     * 
     * Example Usage:
     *   AActor* SomeActor = MyGameSystem->GetSomeActor();
     *   bool bRemoved = Tracker->RemoveSpawnedActorByActor(SomeActor);
     * 
     * Process:
     * 1. Lookup name using ActorToNameMap
     * 2. Call RemoveSpawnedActorByName() with that name
     * 3. Return result
     * 
     * @param Actor - Actor pointer to remove
     * @return true if removed, false if failed
     * 
     * @note: Internally uses RemoveSpawnedActorByName()
     * @note: Safe null checks performed
     * @see RemoveSpawnedActorByName()
     */
    UFUNCTION(BlueprintCallable, Category = "Scene Generation|Actor Management")
    bool RemoveSpawnedActorByActor(AActor* Actor);

    /**
     * Remove multiple actors in one call
     * 
     * Purpose: Batch remove actors by name
     * 
     * Example Usage:
     *   TArray<FString> ToRemove;
     *   ToRemove.Add(TEXT("Chair_01"));
     *   ToRemove.Add(TEXT("Table_02"));
     *   ToRemove.Add(TEXT("Lamp_GenAI_03"));
     *   Tracker->RemoveMultipleActors(ToRemove);
     * 
     * Process:
     * 1. Loop through provided name array
     * 2. Call RemoveSpawnedActorByName() for each
     * 3. Track success/failure count
     * 4. Log summary
     * 
     * Output Example:
     *   ╔════════════════════════════════════╗
     *   ║ 🗑️  RemoveMultipleActors (3)      ║
     *   ╠════════════════════════════════════╣
     *   ║ ✅ Removed: 3                    ║
     *   ║ ❌ Failed: 0                      ║
     *   ╚════════════════════════════════════╝
     * 
     * @param ObjectNames - Array of names to remove
     * 
     * @note: Continues on error (doesn't stop at first failure)
     * @see RemoveSpawnedActorByName()
     */
    UFUNCTION(BlueprintCallable, Category = "Scene Generation|Actor Management")
    void RemoveMultipleActors(const TArray<FString>& ObjectNames);

    /**
     * Clear ALL spawned actors at once
     * 
     * Purpose: Complete cleanup (useful for level transitions)
     * 
     * Example Usage:
     *   // Load new level
     *   Tracker->ClearAllSpawnedActors();
     *   // All spawned actors destroyed
     * 
     * Actions:
     * 1. Loop through SpawnedActors array
     * 2. Destroy each valid actor
     * 3. Clear all three storage containers:
     *    - SpawnedActors.Reset()
     *    - SpawnedActorsByName.Reset()
     *    - ActorToNameMap.Reset()
     * 4. Log destruction count
     * 
     * Output Example:
     *   ╔════════════════════════════════════╗
     *   ║ 🗑️  ClearAllSpawnedActors()      ║
     *   ╠════════════════════════════════════╣
     *   ║ ✅ Destroyed: 5                  ║
     *   ║ ⚠️  Skipped: 0                    ║
     *   ║ 📊 Array Size: 0                  ║
     *   ║ 🗂️  Map Size: 0                   ║
     *   ╚════════════════════════════════════╝
     * 
     * @note: Calls Reset() on TArray and TMap (not Clear())
     * @note: Destroys actors asynchronously
     * @note: Should be called before level transitions
     * @see RemoveSpawnedActorByName() for selective removal
     */
    UFUNCTION(BlueprintCallable, Category = "Scene Generation|Actor Management")
    void ClearAllSpawnedActors();

    // ========================================================================
    // INTERNAL FUNCTIONS - Private Implementation
    // ========================================================================

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
     *   ╔════════════════════════════╗
     *   ║ ✅ ALL SYSTEMS READY!    ║
     *   ║ GenAI requests enabled   ║
     *   ╚════════════════════════════╝
     */
    void CheckSystemsReady();

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

    /**
     * Log current scene state to output log
     * 
     * Called periodically for debugging
     * Shows count of spawned actors and storage usage
     */
    void LogSceneStateVerbose();
};
