// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "LocationQueryEngine.generated.h"

/**
 * FSpawnLocation
 * 
 * Represents a semantic spawn location in the world with full metadata.
 * Mirrors the structure of FTextureSet (AssetIndexer) but for spatial locations.
 * 
 * Architecture:
 *   - Stores named spawn points with semantic context
 *   - Tracks occupancy for runtime spawn management
 *   - Supports LLM descriptions for AI context
 *   - Enables complex spatial queries via tags
 * 
 * Example:
 *   FSpawnLocation EnemySpawn;
 *   EnemySpawn.LocationName = "EnemyRight";
 *   EnemySpawn.WorldPosition = FVector(500, 0, 100);
 *   EnemySpawn.ClearanceRadius = 150.0f;
 *   EnemySpawn.Tags.Add("Arena");
 *   EnemySpawn.Description = "Right side of fighting arena";
 * 
 * @see ULocationQueryEngine
 */
USTRUCT(BlueprintType)
struct FSpawnLocation
{
    GENERATED_BODY()
    
    /// Unique semantic identifier for this location
    /// Examples: "PlayerFront", "ArenaCenter", "Background_Decor_01"
    /// Used for symbolic references in GenAI plans
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Identity")
    FString LocationName;
    
    /// World space coordinates where actors will spawn
    /// Updated during runtime based on level changes
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Transform")
    FVector WorldPosition = FVector::ZeroVector;
    
    /// Orientation/rotation for spawned actors to face
    /// Applied to all actors spawned at this location
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Transform")
    FRotator WorldRotation = FRotator::ZeroRotator;
    
    /// Sphere radius required to be clear for safe spawning (in cm)
    /// Used by IsLocationClear() for collision validation
    /// Typical values: 100-300 depending on actor size
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Safety")
    float ClearanceRadius = 200.0f;
    
    /// Runtime occupancy flag - prevents multiple actors at same spot
    /// Set by SetLocationOccupied() during actor lifecycle
    /// Checked before spawning via IsLocationOccupied()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|State")
    bool bIsOccupied = false;
    
    /// Semantic tags for filtering and categorization
    /// Examples: "Arena", "Background", "Overhead", "Safe", "Elevated"
    /// Similar to AActor::Tags but for locations
    /// Used by GetLocationsByTag() and FindLocationsByTag()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Metadata")
    TArray<FString> Tags;
    
    /// Natural language description for LLM context and documentation
    /// Example: "Right side of arena, suitable for ranged combat enemies"
    /// Used in GetLocationContextForLLM() for prompt generation
    /// Should be descriptive but concise (1-2 sentences)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Metadata")
    FString Description;
    
    /// Actor currently occupying this location (for tracking purposes)
    /// Used to verify occupancy and debug spawn conflicts
    /// TWeakObjectPtr prevents memory leaks if actor is destroyed
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|State")
    TWeakObjectPtr<AActor> OccupyingActor;
};

/**
 * ULocationQueryEngine
 * 
 * Sophisticated spatial location manager for GenAI-driven scene generation.
 * Operates as the location counterpart to UAssetIndexer (handles assets/meshes).
 * 
 * Core Responsibilities:
 *   1. Database Management: Store/retrieve locations with metadata
 *   2. World Scanning: Discover "Loc_*" tagged actors during init
 *   3. Semantic Resolution: Map names ("PlayerFront") → coordinates
 *   4. Validation: Check clearance, occupancy, collision before spawn approval
 *   5. LLM Integration: Provide formatted location context for AI prompts
 *   6. Runtime Tracking: Monitor occupancy and dynamic level changes
 * 
 * Architecture Layers:
 *   - Storage Layer: DiscoveredLocations array + LocationDatabase map
 *   - Query Layer: ResolveLocation(), FindLocationsInRadius(), etc.
 *   - Validation Layer: IsLocationClear(), IsLocationValidForSpawn()
 *   - Resolution Layer: Semantic names → World positions
 *   - Integration Layer: OnLocationScanComplete delegate
 * 
 * Integration Points:
 *   - SceneStateTracker: Calls ScanWorldLocationsAsync() during Init()
 *     Subscribes to OnLocationScanComplete
 *   - GenAISystem: Calls GetLocationContextForLLM() for prompt building
 *     Receives ResolveLocation() calls during plan generation
 *   - SceneBuilder: Uses ResolveLocation() output to spawn actors
 *     Calls SetLocationOccupied() after spawning
 *   - Runtime Systems: Monitor location occupancy, request free spawns
 * 
 * Query Patterns:
 *   
 *   // Simple resolution
 *   FVector SpawnPos = LocationEngine->ResolveLocationName("PlayerFront");
 *   
 *   // Full struct with metadata
 *   FSpawnLocation Loc = LocationEngine->ResolveLocation("PlayerFront");
 *   if (!Loc.bIsOccupied && LocationEngine->IsLocationClear(Loc.WorldPosition, Loc.ClearanceRadius))
 *   {
 *       SpawnActor(Loc);
 *       LocationEngine->SetLocationOccupied(Loc.LocationName, true, NewActor);
 *   }
 *   
 *   // Tag-based queries
 *   TArray<FSpawnLocation> ArenaLocs = LocationEngine->GetLocationsByTag("Arena");
 *   
 *   // Player-relative spawning
 *   FVector FrontPos = LocationEngine->GetPlayerFrontPosition(400.0f);
 *   
 *   // LLM context generation
 *   FString LocContext = LocationEngine->GetLocationContextForLLM();
 * 
 * Level Design Requirements:
 *   - Place actors with "Loc_" prefix in tag name (e.g., "Loc_PlayerLeft")
 *   - Set meaningful actor names (becomes location name after "Loc_" strip)
 *   - Optional: Add semantic tags ("Arena", "Background", etc.) to actors
 * 
 * Performance Considerations:
 *   - IsLocationClear() uses sphere overlap tests (moderate cost)
 *   - Cache results for frequently checked locations
 *   - Limit FindLocationsInRadius() searches to necessary scope
 *   - Use direct map lookup instead of array iteration when possible
 * 
 * @see UAssetIndexer (parallel pattern for asset management)
 * @see USceneStateTracker (calling subsystem)
 * @see UGenAISystem (LLM context consumer)
 */
UCLASS()
class PROJECT_CANVAS_API ULocationQueryEngine : public UObject
{
    GENERATED_BODY()

public:
    // ========================================
    // INITIALIZATION & SCANNING
    // ========================================
    
    /**
     * Scans world for actors tagged with "Loc_*" and builds location database.
     * Should be called ONCE during game initialization (from SceneStateTracker::Init).
     * 
     * Process:
     *   1. Iterates all actors in world
     *   2. Finds those with "Loc_" prefixed tags
     *   3. Extracts position, rotation from actor transform
     *   4. Parses semantic tags and descriptions from actor properties
     *   5. Stores in DiscoveredLocations array + LocationDatabase map
     *   6. Broadcasts OnLocationScanComplete when finished
     * 
     * Timing:
     *   - Async in name only (synchronous execution)
     *   - Completes within frame at init time
     *   - Sets bIsScanComplete = true when done
     * 
     * Logging:
     *   - ✅ Progress: "Starting location scan..."
     *   - ✅ Per-location: "Registered 'LocationName' at position"
     *   - ✅ Summary: "Scan complete. Found X locations."
     * 
     * @param WorldContext World to scan for location actors
     * @return void - Triggers OnLocationScanComplete delegate when done
     * 
     * @see OnLocationScanComplete
     * @see IsScanComplete()
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Init")
    void ScanWorldLocationsAsync(UWorld* WorldContext);
    
    /**
     * Checks if initial world scan is complete.
     * Used by SceneStateTracker to gate GenAI requests.
     * 
     * Returns:
     *   true  = ScanWorldLocationsAsync finished, database is ready
     *   false = Scan not started or still in progress
     * 
     * @return Scan completion status
     * 
     * @see ScanWorldLocationsAsync()
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Init")
    bool IsScanComplete() const { return bIsScanComplete; }

    // ========================================
    // LOCATION DATABASE QUERIES
    // ========================================
    
    /**
     * Gets all discovered location names.
     * Similar to AssetIndexer::GetDiscoveredTextureNames().
     * 
     * Use cases:
     *   - Debug: List all available locations
     *   - GenAI: Build location options for prompt
     *   - UI: Populate location selection dropdowns
     * 
     * @return Array of location identifier strings
     * 
     * Example Output:
     *   ["PlayerLeft", "PlayerCenter", "PlayerRight", "ArenaCenter", 
     *    "BackgroundLeft", "BackgroundRight", "OverheadCenter"]
     * 
     * Performance: O(1) - returns TMap keys
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FString> GetDiscoveredLocationNames() const;
    
    /**
     * Gets all FSpawnLocation structs with full metadata.
     * Use for bulk iteration or complete database access.
     * 
     * @return Complete DiscoveredLocations array
     * 
     * Performance: O(1) - returns reference to array
     * Memory: Large array - avoid copying
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FSpawnLocation> GetAllLocations() const { return DiscoveredLocations; }
    
    /**
     * Filters locations by semantic tag.
     * Used for gameplay queries like "spawn in Arena" or "use Elevated locations"
     * 
     * Tag Examples:
     *   - "Arena": Main fighting area
     *   - "Background": Far away decorative area
     *   - "Overhead": Above player head (for projectiles)
     *   - "Safe": Far from player
     *   - "Elevated": On platforms/high ground
     * 
     * @param Tag Tag to filter by
     * @return Array of locations matching the tag (may be empty)
     * 
     * Performance: O(n) where n = number of locations
     * 
     * @see FindLocationsByTag() for runtime version with logging
     * @see GetDiscoveredLocationTags() to see available tags
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FSpawnLocation> GetLocationsByTag(const FString& Tag) const;
    
    /**
     * Gets all unique tags discovered across locations.
     * Similar to AssetIndexer::GetDiscoveredActorTags().
     * 
     * Use cases:
     *   - Debug: See what tag categories exist
     *   - UI: Populate tag filters
     *   - GenAI: Understand available constraints
     * 
     * @return Array of all discovered tags (no duplicates)
     * 
     * Example Output:
     *   ["Arena", "Background", "Overhead", "Safe", "Elevated", "Combat"]
     * 
     * Performance: O(1) - returns reference to cached array
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FString> GetDiscoveredLocationTags() const;

    // ========================================
    // LOCATION RESOLUTION (Core Function)
    // ========================================
    
    /**
     * Resolves semantic location name to world position.
     * Primary function used during GenAI plan enrichment phase.
     * 
     * Resolution Strategy (priority order):
     *   1. Named database lookup: "PlayerLeft" → stored coordinates
     *   2. Player-relative: "PLAYER_FRONT" → relative to player pawn
     *   3. Custom coordinates: "CUSTOM:[X,Y,Z]" → parse vector
     *   4. Actor-relative: "NEAR:ActorTagName" → find by tag
     *   5. Fallback: Return FVector::ZeroVector if not found
     * 
     * Occupancy Handling:
     *   - Checks if location is occupied
     *   - Does NOT automatically find alternative (use FindNearestFreeLocation)
     *   - Logs warning if occupied
     * 
     * @param LocationName Semantic identifier to resolve
     *   Examples: "PlayerFront", "ArenaCenter", "CUSTOM:[100,200,300]"
     * 
     * @return World position for spawning, or FVector::ZeroVector if failed
     * 
     * Logging:
     *   - ✅ if resolved from database
     *   - ⚠️  if resolved from player-relative
     *   - ⚠️  if occupied (alternative available)
     *   - ❌ if not found (returns zero)
     * 
     * @see ResolveLocation() for full struct
     * @see FindNearestFreeLocation() for fallback alternatives
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Resolve")
    FVector ResolveLocationName(const FString& LocationName);
    
    /**
     * Resolves semantic location name to full FSpawnLocation struct.
     * Used when full metadata is needed (rotation, clearance, description).
     * 
     * Returns complete struct including:
     *   - WorldPosition (from ResolveLocationName)
     *   - WorldRotation (actor orientation)
     *   - ClearanceRadius (required sphere clearance)
     *   - Tags (semantic categories)
     *   - Description (LLM context)
     *   - OccupyingActor (current occupant)
     * 
     * @param LocationName Semantic identifier to resolve
     * @return Complete FSpawnLocation struct with all metadata
     * 
     * Fallback:
     *   - Returns dynamic FSpawnLocation if not in database
     *   - Dynamic location has zero position if unresolvable
     * 
     * Logging:
     *   - ✅ "Found in database"
     *   - ⚠️  "Location occupied (alternative available)"
     *   - ❌ "Could not resolve"
     * 
     * @see ResolveLocationName() for position-only queries
     * @see FindValidSpawnLocation() for occupancy-aware resolution
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Resolve")
    FSpawnLocation ResolveLocation(const FString& LocationName);
    
    /**
     * Checks if location name exists in database.
     * Lightweight validation function.
     * 
     * @param LocationName Name to check
     * @return true if location exists in database, false otherwise
     * 
     * Performance: O(1) - map lookup
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Resolve")
    bool DoesLocationExist(const FString& LocationName) const;

    // ========================================
    // ADD/REMOVE/MODIFY LOCATIONS (Runtime)
    // ========================================
    
    /**
     * Adds new location to database at runtime.
     * Useful for procedural generation or dynamic level modifications.
     * 
     * Side Effects:
     *   - Adds to DiscoveredLocations array
     *   - Updates LocationDatabase map
     *   - Updates DiscoveredLocationTags array
     * 
     * @param NewLocation FSpawnLocation struct with all metadata
     * 
     * Logging:
     *   - ✅ if successfully added
     *   - ⚠️  if duplicate name detected
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    void AddLocation(const FSpawnLocation& NewLocation);
    
    /**
     * Quick add: Creates location from position and name only.
     * Shorthand for AddLocation with basic parameters.
     * Generated struct has defaults for rotation, tags, description.
     * 
     * @param LocationName Identifier for this location
     * @param Position World coordinates
     * @param Clearance Sphere radius required to be clear (default 200cm)
     * 
     * Example:
     *   AddLocationByPosition("SpawnPoint01", FVector(500, 0, 100), 150.0f);
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    void AddLocationByPosition(const FString& LocationName, FVector Position, float Clearance = 200.0f);
    
    /**
     * Removes location from database.
     * Also clears any occupancy markers for that location.
     * 
     * Side Effects:
     *   - Removes from LocationDatabase map
     *   - Removes from DiscoveredLocations array
     *   - Clears occupancy tracking
     * 
     * @param LocationName Location to remove
     * @return true if found and removed, false if not found
     * 
     * Logging:
     *   - ✅ "Removed location X"
     *   - ⚠️  if location not found
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    bool RemoveLocation(const FString& LocationName);
    
    /**
     * Modifies existing location metadata.
     * Updates position, rotation, clearance, tags, description.
     * Preserves occupancy status unless explicitly updated.
     * 
     * @param LocationName Location to modify
     * @param UpdatedLocation New values to apply
     * @return true if found and updated, false if not found
     * 
     * Example:
     *   FSpawnLocation Updated = OldLocation;
     *   Updated.WorldPosition = NewPos;
     *   LocationEngine->ModifyLocation("PlayerLeft", Updated);
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    bool ModifyLocation(const FString& LocationName, const FSpawnLocation& UpdatedLocation);
    
    /**
     * Clears all locations from database.
     * WARNING: Destructive operation - use with caution.
     * Also clears occupancy tracking and tag list.
     * 
     * Use Cases:
     *   - Level transitions
     *   - Dynamic location rebuild
     *   - Debug/testing
     * 
     * Logging:
     *   - Warns with count of cleared locations
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    void ClearAllLocations();

    // ========================================
    // TAG-BASED QUERIES
    // ========================================
    
    /**
     * Finds all locations with a specific tag.
     * Runtime version of GetLocationsByTag with detailed logging.
     * 
     * @param Tag Tag to search for
     * @return Array of matching locations (empty if none found)
     * 
     * Logging:
     *   - Shows count of matches
     *   - ⚠️  warns if no matches
     * 
     * Example:
     *   TArray<FSpawnLocation> ArenaLocs = FindLocationsByTag("Arena");
     *   // Might return 3 locations tagged as "Arena"
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Tags")
    TArray<FSpawnLocation> FindLocationsByTag(const FString& Tag);
    
    /**
     * Adds tag to existing location.
     * No-op if location doesn't exist (logs warning).
     * 
     * @param LocationName Location to tag
     * @param Tag Tag to add
     * 
     * Example:
     *   AddTagToLocation("PlayerLeft", "Safe");
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Tags")
    void AddTagToLocation(const FString& LocationName, const FString& Tag);
    
    /**
     * Removes tag from location.
     * No-op if tag doesn't exist on location.
     * 
     * @param LocationName Location to untag
     * @param Tag Tag to remove
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Tags")
    void RemoveTagFromLocation(const FString& LocationName, const FString& Tag);

    // ========================================
    // PLAYER-RELATIVE QUERIES (2.5D Fighting Game Support)
    // ========================================
    
    /**
     * Gets position in front of player (along forward axis).
     * Useful for spawning enemies or projectiles ahead of player.
     * 
     * Calculation:
     *   PlayerPos + (PlayerForwardVector * Distance)
     * 
     * @param Distance How far ahead of player in cm (default 300)
     * @return World position ahead of player, or zero if no player
     * 
     * Requirements: Player pawn must exist in world
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerFrontPosition(float Distance = 300.0f);
    
    /**
     * Gets position behind player (along backward axis).
     * 
     * Calculation:
     *   PlayerPos - (PlayerForwardVector * Distance)
     * 
     * @param Distance Distance behind player in cm (default 300)
     * @return World position behind player
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerBackPosition(float Distance = 300.0f);
    
    /**
     * Gets position to left of player (along left axis).
     * 
     * Calculation:
     *   PlayerPos - (PlayerRightVector * Distance)
     * 
     * @param Distance Distance to left in cm (default 200)
     * @return World position left of player
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerLeftPosition(float Distance = 200.0f);
    
    /**
     * Gets position to right of player (along right axis).
     * 
     * Calculation:
     *   PlayerPos + (PlayerRightVector * Distance)
     * 
     * @param Distance Distance to right in cm (default 200)
     * @return World position right of player
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerRightPosition(float Distance = 200.0f);
    
    /**
     * Gets current player world position.
     * Used as base for other player-relative queries.
     * 
     * @return Player's current location, or FVector::ZeroVector if no player pawn
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerPosition();
    
    /**
     * Gets random position within distance range from player.
     * Useful for randomized spawn patterns and variety.
     * 
     * Generates point in annulus (ring) around player:
     *   - MinDistance = inner radius
     *   - MaxDistance = outer radius
     * 
     * @param MinDistance Minimum distance from player (default 200cm)
     * @param MaxDistance Maximum distance from player (default 500cm)
     * @return Random position in the annulus
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetRandomPositionNearPlayer(float MinDistance = 200.0f, float MaxDistance = 500.0f);

    // ========================================
    // OCCUPANCY MANAGEMENT
    // ========================================
    
    /**
     * Marks location as occupied/free.
     * Prevents multiple actors from spawning at same location.
     * Call this after successful spawn, before freeing call it before destroy.
     * 
     * @param LocationName Location to mark
     * @param bOccupied true = occupied, false = free
     * @param OccupyingActor Actor occupying location (optional, for tracking)
     * 
     * Logging:
     *   - ✅ when occupation status changes
     *   - ⚠️  if location not found
     * 
     * Example:
     *   // After spawn:
     *   LocationEngine->SetLocationOccupied("PlayerFront", true, NewEnemy);
     *   
     *   // Before destroy:
     *   LocationEngine->SetLocationOccupied("PlayerFront", false);
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Occupancy")
    void SetLocationOccupied(const FString& LocationName, bool bOccupied, AActor* OccupyingActor = nullptr);
    
    /**
     * Checks if location is currently occupied.
     * 
     * @param LocationName Location to check
     * @return true if marked as occupied, false if free or not found
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Occupancy")
    bool IsLocationOccupied(const FString& LocationName) const;
    
    /**
     * Gets all unoccupied locations.
     * Useful for GenAI when selecting from available spawn spots.
     * 
     * @return Array of free FSpawnLocation structs
     * 
     * Performance: O(n) where n = total locations
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Occupancy")
    TArray<FSpawnLocation> GetFreeLocations() const;
    
    /**
     * Clears all occupancy markers.
     * Resets system to believe all locations are free.
     * Use after level reset or when doing bulk spawn clear.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Occupancy")
    void ClearAllOccupancy();

    // ========================================
    // COLLISION & VALIDATION
    // ========================================
    
    /**
     * Finds nearest free (unoccupied) location from preferred position.
     * Used as fallback when preferred location is blocked.
     * 
     * Algorithm:
     *   1. Check if preferred location is free
     *   2. If taken, search in expanding concentric rings
     *   3. Return first free location with sufficient clearance
     *   4. Fallback to preferred if nothing found (may be blocked)
     * 
     * Search Pattern:
     *   - Searches in rings at 250cm intervals
     *   - 12 samples per ring (30° apart)
     *   - Up to 5 rings (max 1250cm away)
     * 
     * @param PreferredLocation Desired spawn point
     * @param MinClearance Minimum required sphere clearance (default 150cm)
     * @return Nearest available location (fallback if all blocked)
     * 
     * Logging:
     *   - ✅ shows distance to found location
     *   - ⚠️  if had to search for alternative
     * 
     * Performance: Expensive - avoid calling every frame
     * 
     * @see ResolveLocation() uses this for alternatives
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    FSpawnLocation FindNearestFreeLocation(FVector PreferredLocation, float MinClearance = 150.0f);
    
    /**
     * Checks if spherical area has sufficient clearance.
     * Performs sphere overlap test to detect obstruction.
     * 
     * @param Location Center of clearance sphere
     * @param Radius Sphere radius to check in cm
     * @return true if area is clear (no blocking geometry)
     * 
     * Performance: Expensive - uses physics queries
     * Optimization: Cache results for frequently checked locations
     * 
     * Channel: Uses ECC_WorldStatic (world geometry)
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    bool IsLocationClear(FVector Location, float Radius) const;
    
    /**
     * Snaps position to ground by tracing downward.
     * Adjusts Z coordinate (vertical) to ground level.
     * 
     * Process:
     *   1. Starts trace at Location + 100cm up
     *   2. Traces down to Location - MaxTraceDistance
     *   3. Returns hit location, or original if no hit
     * 
     * @param Location Position to snap
     * @param MaxTraceDistance How far to trace down (default 1000cm)
     * @return Snapped position with Z adjusted to ground
     * 
     * Use Case:
     *   After random spawn generation to ensure actors stand on ground
     *   FVector SnappedPos = LocationEngine->SnapToGround(RandomPos);
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    FVector SnapToGround(FVector Location, float MaxTraceDistance = 1000.0f);

    // ========================================
    // LLM INTEGRATION & CONTEXT
    // ========================================
    
    /**
     * Generates formatted location context string for LLM prompts.
     * Used by GenAISystem to build system message/initial context.
     * 
     * Output Format Example:
     *   "=== AVAILABLE SPAWN LOCATIONS ===
     *    
     *    [Named Locations]
     *    - \"PlayerLeft\": Left side of arena position [OCCUPIED]
     *    - \"PlayerCenter\": Center arena position
     *    - \"ArenaCenter\": Dead center arena
     *    
     *    [Player-Relative]
     *    - \"PLAYER_FRONT\", \"PLAYER_BACK\", ...
     *    
     *    [Custom Coordinates]
     *    - Format: \"CUSTOM:[X,Y,Z]\""
     * 
     * Features:
     *   - Lists all named locations with descriptions
     *   - Shows occupancy status
     *   - Documents player-relative options
     *   - Explains custom coordinate format
     * 
     * @return Formatted string suitable for LLM prompts
     * 
     * @see BuildLocationDatabase() for structured data version
     * @see GetLocationContextForLLM() for prompt inclusion
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|LLM")
    FString GetLocationContextForLLM() const;
    
    /**
     * Builds structured location database for advanced LLM processing.
     * Returns TMap for programmatic iteration in GenAI logic.
     * 
     * @return Map of LocationName → FSpawnLocation for all locations
     * 
     * Use Cases:
     *   - GenAI needs to iterate locations programmatically
     *   - Building JSON export for external systems
     *   - Complex filtering logic
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|LLM")
    TMap<FString, FSpawnLocation> BuildLocationDatabase();

    // ========================================
    // EVENTS & DELEGATES
    // ========================================
    
    /// Broadcast when ScanWorldLocationsAsync completes
    /// Subscribed to by: SceneStateTracker::OnLocationScanFinished()
    /// Used to gate: GenAI system initialization until locations ready
    FSimpleDelegate OnLocationScanComplete;
// ========================================
// DEBUG & VISUALIZATION FUNCTIONS
// ========================================

/**
 * Prints complete database report to log.
 * 
 * Output includes:
 *   - Total location count
 *   - Discovered tags count
 *   - Per-location details:
 *     * Index and name
 *     * Occupancy status (🔴 occupied or 🟢 free)
 *     * Clearance radius
 *     * World position coordinates
 *     * All tags assigned
 *     * Description text
 * 
 * Use Cases:
 *   - Debug: Verify locations loaded correctly
 *   - Testing: Check occupancy state at breakpoint
 *   - Validation: Ensure position coordinates are sensible
 * 
 * Logging Level: Display (verbose)
 * Performance: O(n) where n = number of locations
 * 
 * Example Output:
 *   ╔═══════════════════════════════════════════╗
 *   ║   📊 Location Database Report             ║
 *   ╚═══════════════════════════════════════════╝
 *   Total Locations: 5
 *   Discovered Tags: 3
 *   [1] PlayerLeft | 🟢 FREE | Clearance: 200.0
 *        Position: [100, 0, 100]
 *        Tags: 1 | Arena
 * 
 * @see PrintLocationsByStatus() for occupancy-focused report
 * @see VisualizeAllLocations() for visual representation
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
void PrintAllLocationData() const;

/**
 * Visualizes all locations in world with debug geometry.
 * Draws spheres at each location with color-coded occupancy status.
 * 
 * Visual Elements:
 *   - Green sphere: Free location (radius = ClearanceRadius)
 *   - Red sphere: Occupied location
 *   - White text label: Location name above sphere
 *   - Number of segments: 16 (sphere resolution)
 * 
 * @param Duration How long to display visualization in seconds (default 5.0)
 * 
 * Use Cases:
 *   - Level design: Verify spawn points are well-placed
 *   - Debugging: Check collision/clearance visually
 *   - Performance analysis: See spatial distribution
 * 
 * Performance: Temporary draw calls - no persistent cost
 * Channel: Visibility (ECC_Visibility)
 * 
 * Example:
 *   LocationEngine->VisualizeAllLocations(10.0f);
 *   // Shows all locations for 10 seconds in editor viewport
 * 
 * @see PrintAllLocationData() for text report version
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
void VisualizeAllLocations(float Duration = 5.0f);

/**
 * Prints occupancy status summary to log.
 * Lightweight report focused on free vs occupied counts.
 * 
 * Output Format:
 *   📊 Location Status Report:
 *   🔴 EnemyLeft - OCCUPIED
 *   🔴 ArenaCenter - OCCUPIED
 *   🟢 5 free locations
 *   🔴 2 occupied locations
 * 
 * Use Cases:
 *   - Quick status check during gameplay
 *   - Monitoring spawn availability
 *   - Detecting occupancy bugs
 * 
 * Performance: O(n) lightweight scan
 * Logging Level: Display
 * 
 * @see PrintAllLocationData() for detailed report
 * @see GetFreeLocationCount() for programmatic access
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
void PrintLocationsByStatus() const;

// ========================================
// BATCH OPERATIONS
// ========================================

/**
 * Adds multiple locations at once from array.
 * Wrapper around AddLocation() for bulk operations.
 * 
 * Behavior:
 *   - Iterates array and calls AddLocation() for each
 *   - Updates database, array, and tags for all
 *   - Logs progress and completion
 * 
 * @param NewLocations Array of FSpawnLocation structs to add
 * 
 * Use Cases:
 *   - Loading level-specific locations from data
 *   - Procedural generation batches
 *   - Initializing from external format
 * 
 * Logging:
 *   - 🔄 "Adding X locations..."
 *   - ✅ "Batch add complete: Y total"
 * 
 * Performance: O(n) where n = NewLocations.Num()
 * 
 * Example:
 *   TArray<FSpawnLocation> Batch;
 *   // ... populate Batch
 *   LocationEngine->AddMultipleLocations(Batch);
 * 
 * @see AddLocation() for single additions
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
void AddMultipleLocations(const TArray<FSpawnLocation>& NewLocations);

/**
 * Removes multiple locations at once from array of names.
 * Wrapper around RemoveLocation() for bulk deletion.
 * 
 * Behavior:
 *   - Iterates names and calls RemoveLocation() for each
 *   - Clears from database, array, and occupancy tracking
 *   - Counts successes and reports
 * 
 * @param LocationNames Array of names to remove
 * @return true if all locations removed successfully, false if any not found
 * 
 * Use Cases:
 *   - Batch cleanup of temporary spawn points
 *   - Level transition clearing
 *   - Procedural generation teardown
 * 
 * Logging:
 *   - 🗑️  "Removing X locations..."
 *   - ✅ "Removed: Y/Z" (Y successful, Z total)
 * 
 * Performance: O(n*m) where n = count, m = database size
 * 
 * @see RemoveLocation() for single removal
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
bool RemoveMultipleLocations(const TArray<FString>& LocationNames);

/**
 * Renames existing location.
 * Updates name in database, array, and occupancy tracking.
 * 
 * Process:
 *   1. Find location by OldName (case-insensitive)
 *   2. Copy to new struct with NewName
 *   3. Remove old entry
 *   4. Add as new entry
 * 
 * @param OldName Current location identifier
 * @param NewName New identifier for this location
 * 
 * Side Effects:
 *   - Updates LocationDatabase key
 *   - Updates DiscoveredLocations array
 *   - Updates occupancy tracking key
 * 
 * Use Cases:
 *   - Fix naming mistakes
 *   - Reorganize location scheme
 *   - Dynamic naming based on conditions
 * 
 * Logging:
 *   - ✅ "Renamed 'Old' → 'New'"
 *   - ⚠️  if OldName not found
 * 
 * Warning: Will break any references to OldName!
 * 
 * @see RemoveLocation() then AddLocation() as alternative
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
void RenameLocation(const FString& OldName, const FString& NewName);

// ========================================
// ADVANCED QUERIES
// ========================================

/**
 * Finds location farthest from given position.
 * Useful for spreading out spawns or escaping patterns.
 * 
 * Algorithm:
 *   - Iterate all locations
 *   - Calculate distance to each from position
 *   - Return location with maximum distance
 * 
 * @param FromPosition Reference point (typically player position)
 * @return FSpawnLocation of farthest point, or empty if no locations
 * 
 * Use Cases:
 *   - Spawn away from player ("safe" spawn)
 *   - Evade patterns
 *   - Spread enemy spawn points
 * 
 * Performance: O(n) where n = total locations
 * 
 * Logging:
 *   - 🔍 "Farthest from [X,Y,Z]: LocationName (Distance units)"
 *   - ⚠️  if no locations available
 * 
 * Example:
 *   FSpawnLocation SafeSpot = LocationEngine->FindFarthestLocation(PlayerPos);
 * 
 * @see FindClosestLocation() for opposite
 * @see FindLocationsInRadius() for area-based search
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
FSpawnLocation FindFarthestLocation(FVector FromPosition) const;

/**
 * Finds location closest to given position.
 * Inverse of FindFarthestLocation().
 * 
 * Algorithm:
 *   - Iterate all locations
 *   - Calculate distance to each from position
 *   - Return location with minimum distance
 * 
 * @param FromPosition Reference point
 * @return FSpawnLocation of closest point
 * 
 * Use Cases:
 *   - Quick spawn near player
 *   - Follow/intercept patterns
 *   - Nearest neighbor queries
 * 
 * Performance: O(n) where n = total locations
 * 
 * Logging:
 *   - 🔍 "Closest to [X,Y,Z]: LocationName (Distance units)"
 * 
 * @see FindFarthestLocation() for opposite
 * @see FindNearestFreeLocation() for occupancy-aware version
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
FSpawnLocation FindClosestLocation(FVector FromPosition) const;

/**
 * Finds all locations within distance radius from center point.
 * Returns array of candidates for area-based operations.
 * 
 * Algorithm:
 *   - Iterate all locations
 *   - Calculate distance from Center to each
 *   - Add to result if Distance <= Radius
 * 
 * @param Center Point to search around
 * @param Radius Search radius in cm
 * @return Array of locations within radius (empty if none)
 * 
 * Use Cases:
 *   - Area effect spawning ("spawn 3 enemies around boss")
 *   - Regional queries ("all Arena locations")
 *   - Proximity checks
 * 
 * Performance: O(n) where n = total locations
 * Performance Tips:
 *   - Pre-compute regions/sectors if called frequently
 *   - Consider spatial hashing for large location counts
 * 
 * Logging:
 *   - 🔍 "Found X locations within Y radius"
 * 
 * Example:
 *   TArray<FSpawnLocation> NearbySpots = 
 *       LocationEngine->FindLocationsInRadius(BossPos, 1000.0f);
 * 
 * @see FindClosestLocation() for single search
 * @see GetLocationsByTag() for category-based filtering
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
TArray<FSpawnLocation> FindLocationsInRadius(FVector Center, float Radius) const;

/**
 * Gets count of unoccupied locations.
 * Lightweight query for availability checks.
 * 
 * @return Number of locations with bIsOccupied == false
 * 
 * Use Cases:
 *   - Check if spawning possible
 *   - Gate GenAI requests if no free spots
 *   - Debug occupancy status
 * 
 * Performance: O(n) single pass
 * 
 * @see GetOccupiedLocationCount() for inverse
 * @see GetFreeLocations() for actual array
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
int32 GetFreeLocationCount() const;

/**
 * Gets count of occupied locations.
 * Inverse of GetFreeLocationCount().
 * 
 * Equivalent to: DiscoveredLocations.Num() - GetFreeLocationCount()
 * 
 * @return Number of locations with bIsOccupied == true
 * 
 * Performance: O(n) calculated from free count
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
int32 GetOccupiedLocationCount() const;

// ========================================
// VALIDATION ENHANCEMENTS
// ========================================

/**
 * Checks if location is valid for spawning.
 * Comprehensive validation covering all conditions.
 * 
 * Validation Checks (in order):
 *   1. Location exists in database
 *   2. Location is not occupied
 *   3. Clearance radius meets minimum
 *   4. No collision at location
 * 
 * @param LocationName Location to validate
 * @param MinClearance Minimum required clearance radius (default 150cm)
 * @return true if all checks pass
 * 
 * Use Cases:
 *   - Pre-spawn validation
 *   - GenAI decision making
 *   - Debug spawn failures
 * 
 * Logging:
 *   - ❌ if location doesn't exist
 *   - ⚠️  if occupied
 *   - ⚠️  if insufficient clearance
 *   - ⚠️  if obstructed by geometry
 * 
 * Performance: Expensive (includes collision test)
 * Optimization: Cache results for frequently checked locations
 * 
 * Example:
 *   if (LocationEngine->IsLocationValidForSpawn("EnemyFront", 200.0f))
 *   {
 *       SpawnEnemy("EnemyFront");
 *   }
 * 
 * @see FindValidSpawnLocation() for fallback version
 * @see IsLocationClear() for collision-only check
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
bool IsLocationValidForSpawn(const FString& LocationName, float MinClearance = 150.0f) const;

/**
 * Finds valid spawn location, falling back if preferred is invalid.
 * Combines preference with validation logic.
 * 
 * Algorithm:
 *   1. Try preferred location with IsLocationValidForSpawn()
 *   2. If valid, return it
 *   3. If invalid, search all locations for first valid one
 *   4. If none valid, return empty FSpawnLocation
 * 
 * @param PreferredLocation Location to try first
 * @param MinClearance Minimum clearance requirement (default 150cm)
 * @return Valid FSpawnLocation or empty if none found
 * 
 * Use Cases:
 *   - Preferred spawn with fallback
 *   - GenAI planning with automatic alternatives
 *   - Robust spawn system
 * 
 * Logging:
 *   - ⚠️  if preferred invalid, searching...
 *   - ✅ "Found alternative: LocationName"
 *   - ❌ if no valid location found
 * 
 * Performance: O(n) worst case
 * 
 * @see IsLocationValidForSpawn() for validation-only check
 * @see FindNearestFreeLocation() for spatial alternative search
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
FSpawnLocation FindValidSpawnLocation(const FString& PreferredLocation, float MinClearance = 150.0f);

// ========================================
// STATISTICS & UTILITY FUNCTIONS
// ========================================

/**
 * Calculates average distance between all location pairs.
 * Metric for spatial distribution.
 * 
 * Calculation:
 *   - Compute distance between every pair (i,j where i<j)
 *   - Average all distances
 * 
 * @return Average distance in cm, or 0.0 if fewer than 2 locations
 * 
 * Use Cases:
 *   - Level design: Check spawn spread
 *   - Performance: Estimate spatial queries cost
 *   - Debugging: Verify reasonable distribution
 * 
 * Performance: O(n²) where n = number of locations
 * Warning: Expensive for large location counts!
 * 
 * Example:
 *   float AvgDist = LocationEngine->GetAverageLocationDistance();
 *   if (AvgDist < 500.0f) UE_LOG(..., "Locations clustered!");
 * 
 * @see GetLocationCentroid() for center of mass
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Utility")
float GetAverageLocationDistance() const;

/**
 * Calculates centroid (center of mass) of all locations.
 * Useful for finding layout center point.
 * 
 * Calculation:
 *   Sum all positions / number of locations
 * 
 * @return FVector representing average position of all locations
 * 
 * Use Cases:
 *   - Find arena center for camera placement
 *   - Spatial queries reference point
 *   - Level balance analysis
 * 
 * Performance: O(n) single pass
 * Returns: FVector::ZeroVector if no locations
 * 
 * @see FindClosestLocation() for nearest to centroid
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Utility")
FVector GetLocationCentroid() const;

/**
 * Prints comprehensive location statistics to log.
 * Summary of counts, distances, and spatial info.
 * 
 * Output Includes:
 *   - Total location count
 *   - Free vs occupied breakdown
 *   - Average distance between locations
 *   - Centroid position
 * 
 * Use Cases:
 *   - Level analysis
 *   - Debug at level start
 *   - Performance profiling
 * 
 * Logging Level: Display
 * Performance: O(n²) due to average distance calculation
 * 
 * Example Output:
 *   📈 Location Statistics:
 *   Total: 10
 *   Free: 7
 *   Occupied: 3
 *   Avg Distance: 523.45 units
 *   Centroid: [250, 0, 100]
 * 
 * @see PrintAllLocationData() for detailed database dump
 * @see PrintLocationsByStatus() for occupancy focus
 */
UFUNCTION(BlueprintCallable, Category = "LocationEngine|Utility")
void PrintLocationStats() const;


    void BeginDestroy() override;
private:
    // ========================================
    // INTERNAL HELPERS
    // ========================================
    
    /// Scans world for "Loc_*" tagged actors and extracts location data
    void ScanForNamedLocations(UWorld* InWorldContext);
    
    /// Parses custom coordinate format: "CUSTOM:[X,Y,Z]" → FVector
    /// Example: "CUSTOM:[100,200,300]" → FVector(100, 200, 300)
    FVector ParseCustomCoordinate(const FString& CoordString) const;
    
    /// Gets player pawn with null check and logging
    /// Returns nullptr if no player controller or no pawn possessed
    APawn* GetPlayerPawn() const;
    
    /// Finds actor by tag name in the world
    /// Returns first valid actor matching tag
    AActor* FindActorWithTag(const FString& Tag) const;

    // ========================================
    // DATA STORAGE
    // ========================================
    
    /// Master array of all discovered locations
    /// Parallel to: AssetIndexer::DiscoveredTextures
    /// Accessed by: Iteration, bulk operations, GetAllLocations()
    UPROPERTY()
    TArray<FSpawnLocation> DiscoveredLocations;
    
    /// Fast lookup map: LocationName → FSpawnLocation
    /// Parallel to: AssetIndexer::TextureDatabase
    /// Used by: ResolveLocation() for O(1) lookups
    /// Key format: UPPERCASE (case-insensitive matching)
    UPROPERTY()
    TMap<FString, FSpawnLocation> LocationDatabase;
    
    /// All unique tags found across locations
    /// Parallel to: AssetIndexer::DiscoveredActorTags
    /// Updated when: Tags added via AddTagToLocation() or AddLocation()
    /// Used by: GetDiscoveredLocationTags()
    UPROPERTY()
    TArray<FString> DiscoveredLocationTags;
    
    /// Occupancy tracking: LocationName → Occupying Actor
    /// Updated by: SetLocationOccupied()
    /// Queried by: IsLocationOccupied(), GetFreeLocations()
    UPROPERTY()
    TMap<FString, AActor*> LocationOccupancy;
    
    /// World context for line traces and actor queries
    /// Set by: ScanWorldLocationsAsync()
    /// Used by: IsLocationClear(), SnapToGround(), GetPlayerPawn()
    UPROPERTY()
    UWorld* WorldContext = nullptr;
    
    /// Flag: Initial scan complete?
    /// Parallel to: AssetIndexer::bScanComplete
    /// Set by: ScanWorldLocationsAsync() when finished
    /// Read by: IsScanComplete()
    UPROPERTY()
    bool bIsScanComplete = false;
    
    /// Flag: Scan in progress?
    /// Used for safety checks and preventing concurrent scans
    UPROPERTY()
    bool bIsScanning = false;
};
