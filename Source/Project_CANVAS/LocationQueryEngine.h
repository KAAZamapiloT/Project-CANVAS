// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Spatial/PointHashGrid3.h"
#include "LocationResolverLLM.h"


#include "LocationQueryEngine.generated.h"





USTRUCT()
struct FActorArrayWrapper
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<AActor*> Actors;
    
    // Optional: Helpers to make code cleaner
    void Add(AActor* Actor) { Actors.AddUnique(Actor); }
    void Empty() { Actors.Empty(); }
    int32 Num() const { return Actors.Num(); }
};

/**
 * Represents a semantic spawn location in the world with full metadata.
 * Stores named spawn points with context, occupancy, and queryable tags.
 * @see ULocationQueryEngine
 */
USTRUCT(BlueprintType)
struct FSpawnLocation
{
    GENERATED_BODY()

    /**
     * Unique semantic identifier for this location, used for GenAI plans.
     * Examples: "PlayerFront", "ArenaCenter".
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Identity")
    FString LocationName;

    /** World space coordinates where actors will spawn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Transform")
    FVector WorldPosition = FVector::ZeroVector;

    /** Orientation/rotation for spawned actors to face. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Transform")
    FRotator WorldRotation = FRotator::ZeroRotator;

    /** Sphere radius required to be clear for safe spawning (in cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Safety")
    float ClearanceRadius = 200.0f;

    /**
     * Runtime flag to prevent multiple actors spawning here.
     * Managed by SetLocationOccupied().
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|State")
    bool bIsOccupied = false;

    /**
     * Semantic tags for filtering and categorization.
     * Examples: "Arena", "Background", "Elevated".
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Metadata")
    TArray<FString> Tags;

    /**
     * Natural language description for LLM context and documentation.
     * Example: "Right side of arena, suitable for ranged combat."
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|Metadata")
    FString Description;

    /** Actor currently occupying this location (for tracking purposes). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location|State")
    TWeakObjectPtr<AActor> OccupyingActor;
};

/**
 * Sophisticated spatial location manager for GenAI-driven scene generation.
 *
 * Responsible for discovering, storing, and querying FSpawnLocation points
 * (marked by "Loc_*" tags in the world). It validates locations for
 * occupancy and clearance, resolves semantic names (e.g., "PlayerFront")
 * to coordinates, and provides context for the GenAI system.
 *
 * @see UAssetIndexer (parallel pattern for asset management)
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
     * Scans the world for actors tagged with "Loc_*" to build the location database.
     * This is the primary initialization function and should be called once at startup.
     * Broadcasts OnLocationScanComplete when finished.
     *
     * @param WorldContext The world to scan for location actors.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Init")
    void ScanWorldLocationsAsync(UWorld* WorldContext);

    /**
     * Checks if the initial world scan has completed.
     * @return true if the database is ready, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Init")
    bool IsScanComplete() const { return bIsScanComplete; }

    // ========================================
    // LOCATION DATABASE QUERIES
    // ========================================

    /**
     * Gets the names of all discovered locations.
     * @return Array of location identifier strings.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FString> GetDiscoveredLocationNames() const;

    /**
     * Gets all discovered FSpawnLocation structs with full metadata.
     * @return The complete array of all stored locations.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FSpawnLocation> GetAllLocations() const { return DiscoveredLocations; }

    /**
     * Filters and returns all locations that contain the specified tag.
     * @param Tag The tag to filter by (e.g., "Arena", "Background").
     * @return Array of locations matching the tag.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FSpawnLocation> GetLocationsByTag(const FString& Tag) const;

    /**
     * Gets all unique tags discovered across all locations.
     * @return Array of all unique discovered tags.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FString> GetDiscoveredLocationTags() const;

    // ========================================
    // LOCATION RESOLUTION (Core Function)
    // ========================================

    /**
     * Resolves a semantic location name to its world position (FVector).
     * Handles named locations ("PlayerLeft"), player-relative ("PLAYER_FRONT"),
     * and custom coordinates ("CUSTOM:[X,Y,Z]").
     *
     * @param LocationName Semantic identifier to resolve.
     * @return World position for spawning, or FVector::ZeroVector if failed.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Resolve")
    FVector ResolveLocationName(const FString& LocationName);

    /**
     * Resolves a semantic location name to the full FSpawnLocation struct.
     * Use this when metadata (rotation, clearance, tags) is needed.
     *
     * @param LocationName Semantic identifier to resolve.
     * @return Complete FSpawnLocation struct. Returns a dynamic or zeroed
     * struct if the name is not in the database but is resolvable
     * (e.g., "PLAYER_FRONT").
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Resolve")
    FSpawnLocation ResolveLocation(const FString& LocationName);

    /**
     * Checks if a location name exists in the database.
     * @param LocationName Name to check.
     * @return true if the location exists, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Resolve")
    bool DoesLocationExist(const FString& LocationName) const;

    // ========================================
    // ADD/REMOVE/MODIFY LOCATIONS (Runtime)
    // ========================================

    /**
     * Adds a new location to the database at runtime.
     * @param NewLocation The FSpawnLocation struct to add.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    void AddLocation(const FSpawnLocation& NewLocation);

    /**
     * Shorthand to add a new location using only its name, position, and clearance.
     *
     * @param LocationName  Identifier for this location.
     * @param Position      World coordinates.
     * @param Clearance     Sphere radius required to be clear (default 200cm).
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    void AddLocationByPosition(const FString& LocationName, FVector Position, float Clearance = 200.0f);

    /**
     * Removes a location from the database.
     * @param LocationName The identifier of the location to remove.
     * @return true if found and removed, false if not found.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    bool RemoveLocation(const FString& LocationName);

    /**
     * Modifies an existing location's metadata.
     * @param LocationName      The name of the location to modify.
     * @param UpdatedLocation   The FSpawnLocation struct with the new data.
     * @return true if found and updated, false if not found.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    bool ModifyLocation(const FString& LocationName, const FSpawnLocation& UpdatedLocation);

    /** Clears all locations from the database. Use with caution. */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    void ClearAllLocations();

    // ========================================
    // TAG-BASED QUERIES
    // ========================================

    /**
     * Finds all locations with a specific tag (runtime version of GetLocationsByTag).
     * @param Tag Tag to search for.
     * @return Array of matching locations.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Tags")
    TArray<FSpawnLocation> FindLocationsByTag(const FString& Tag);

    /**
     * Adds a new tag to an existing location.
     * @param LocationName  Location to tag.
     * @param Tag           Tag to add.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Tags")
    void AddTagToLocation(const FString& LocationName, const FString& Tag);

    /**
     * Removes a tag from an existing location.
     * @param LocationName  Location to untag.
     * @param Tag           Tag to remove.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Tags")
    void RemoveTagFromLocation(const FString& LocationName, const FString& Tag);

    // ========================================
    // PLAYER-RELATIVE QUERIES (2.5D Fighting Game Support)
    // ========================================

    /**
     * Gets a position in front of the player pawn.
     * @param Distance How far ahead of player in cm (default 300).
     * @return World position ahead of player, or zero if no player.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerFrontPosition(float Distance = 300.0f);

    /**
     * Gets a position behind the player pawn.
     * @param Distance Distance behind player in cm (default 300).
     * @return World position behind player.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerBackPosition(float Distance = 300.0f);

    /**
     * Gets a position to the left of the player pawn.
     * @param Distance Distance to left in cm (default 200).
     * @return World position left of player.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerLeftPosition(float Distance = 200.0f);

    /**
     * Gets a position to the right of the player pawn.
     * @param Distance Distance to right in cm (default 200).
     * @return World position right of player.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerRightPosition(float Distance = 200.0f);

    /**
     * Gets the current player pawn's world position.
     * @return Player's location, or FVector::ZeroVector if no player pawn.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetPlayerPosition() const;

    /**
     * Gets a random position within an annulus (ring) around the player.
     * @param MinDistance Minimum distance from player (default 200cm).
     * @param MaxDistance Maximum distance from player (default 500cm).
     * @return Random position in the annulus.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Player")
    FVector GetRandomPositionNearPlayer(float MinDistance = 200.0f, float MaxDistance = 500.0f);

    // ========================================
    // OCCUPANCY MANAGEMENT
    // ========================================

    /**
     * Marks a location as occupied or free.
     * Call this after spawning an actor or before destroying it.
     *
     * @param LocationName      Location to mark.
     * @param bOccupied         true = occupied, false = free.
     * @param OccupyingActor    Actor occupying location (optional, for tracking).
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Occupancy")
    void SetLocationOccupied(const FString& LocationName, bool bOccupied, AActor* OccupyingActor = nullptr);

    /**
     * Checks if a location is currently marked as occupied.
     * @param LocationName Location to check.
     * @return true if occupied, false if free or not found.
     */
   // UFUNCTION(BlueprintCallable, Category = "LocationEngine|Occupancy")
    //bool IsLocationOccupied(const FString& LocationName) const;

    /**
     * Gets all locations currently marked as unoccupied.
     * @return Array of free FSpawnLocation structs.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Occupancy")
    TArray<FSpawnLocation> GetFreeLocations() const;

    /**
     * Clears all occupancy markers, setting all locations to free.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Occupancy")
    void ClearAllOccupancy();

    // ========================================
    // COLLISION & VALIDATION
    // ========================================

    /**
     * Finds the nearest free (unoccupied and clear) location from a preferred position.
     * Used as a fallback when the desired spot is blocked.
     *
     * @param PreferredLocation Desired spawn point.
     * @param MinClearance      Minimum required sphere clearance (default 150cm).
     * @return Nearest available location.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    FSpawnLocation FindNearestFreeLocation(FVector PreferredLocation, float MinClearance = 150.0f);

    /**
     * Checks if a spherical area is clear of blocking geometry (ECC_WorldStatic).
     * @param Location  Center of the clearance sphere.
     * @param Radius    Sphere radius to check in cm.
     * @return true if area is clear.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    bool IsLocationClear(FVector Location, float Radius) const;

    /**
     * Snaps a position to the ground by tracing downward.
     * @param Location          Position to snap.
     * @param MaxTraceDistance  How far to trace down (default 1000cm).
     * @return Snapped position with Z adjusted to ground, or original if no hit.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    FVector SnapToGround(FVector Location, float MaxTraceDistance = 1000.0f);

    // ========================================
    // LLM INTEGRATION & CONTEXT
    // ========================================

    /**
     * Generates a formatted location context string for LLM prompts.
     * Lists named locations, occupancy, and dynamic options.
     *
     * @return Formatted string suitable for LLM prompts.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|LLM")
    FString GetLocationContextForLLM() const;

    /**
     * Builds a structured location database map (Name -> Struct).
     * @return TMap of LocationName -> FSpawnLocation for all locations.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|LLM")
    TMap<FString, FSpawnLocation> BuildLocationDatabase();

    // ========================================
    // EVENTS & DELEGATES
    // ========================================

    /** Broadcast when ScanWorldLocationsAsync completes. */
    FSimpleDelegate OnLocationScanComplete;

    // ========================================
    // DEBUG & VISUALIZATION FUNCTIONS
    // ========================================

    /**
     * Prints a comprehensive report of the entire location database to the log.
     * Includes counts, tags, and details for each location.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
    void PrintAllLocationData() const;

    /**
     * Visualizes all locations in the world with debug spheres.
     * Green = Free, Red = Occupied. Also displays location name.
     *
     * @param Duration How long to display visualization in seconds (default 5.0).
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
    void VisualizeAllLocations(float Duration = 5.0f);

    /**
     * Prints a lightweight occupancy status summary to the log.
     * Shows free vs. occupied counts and lists occupied locations.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
    void PrintLocationsByStatus() const;

    // ========================================
    // BATCH OPERATIONS
    // ========================================

    /**
     * Adds multiple locations at once from an array.
     * @param NewLocations Array of FSpawnLocation structs to add.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    void AddMultipleLocations(const TArray<FSpawnLocation>& NewLocations);

    /**
     * Removes multiple locations at once from an array of names.
     * @param LocationNames Array of names to remove.
     * @return true if all locations were found and removed.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    bool RemoveMultipleLocations(const TArray<FString>& LocationNames);

    /**
     * Renames an existing location.
     * @param OldName Current location identifier.
     * @param NewName New identifier for this location.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Manage")
    void RenameLocation(const FString& OldName, const FString& NewName);

    // ========================================
    // ADVANCED QUERIES
    // ========================================

    /**
     * Finds the location farthest from a given position.
     * @param FromPosition Reference point (e.g., player position).
     * @return FSpawnLocation of the farthest point.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
    FSpawnLocation FindFarthestLocation(FVector FromPosition) const;

    /**
     * Finds the location closest to a given position.
     * @param FromPosition Reference point.
     * @return FSpawnLocation of the closest point.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
    FSpawnLocation FindClosestLocation(FVector FromPosition) const;

    /**
     * Finds all locations within a distance radius from a center point.
     * @param Center Point to search around.
     * @param Radius Search radius in cm.
     * @return Array of locations within the radius.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
    TArray<FSpawnLocation> FindLocationsInRadius(FVector Center, float Radius) const;

    /**
     * Gets the count of unoccupied locations.
     * @return Number of locations with bIsOccupied == false.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
    int32 GetFreeLocationCount() const;

    /**
     * Gets the count of occupied locations.
     * @return Number of locations with bIsOccupied == true.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Advanced")
    int32 GetOccupiedLocationCount() const;

    // ========================================
    // VALIDATION ENHANCEMENTS
    // ========================================

    /**
     * Checks if a location is valid for spawning (exists, not occupied, clear).
     * @param LocationName    Location to validate.
     * @param MinClearance    Minimum required clearance radius (default 150cm).
     * @return true if all checks pass.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    bool IsLocationValidForSpawn(const FString& LocationName, float MinClearance = 150.0f) const;

    /**
     * Finds a valid spawn location, falling back if the preferred one is invalid.
     * @param PreferredLocation Location to try first.
     * @param MinClearance      Minimum clearance requirement (default 150cm).
     * @return A valid FSpawnLocation, or an empty/zeroed struct if none found.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    FSpawnLocation FindValidSpawnLocation(const FString& PreferredLocation, float MinClearance = 150.0f);

    // ========================================
    // STATISTICS & UTILITY FUNCTIONS
    // ========================================

    /**
     * Calculates the average distance between all location pairs.
     * Expensive O(n^2) operation.
     * @return Average distance in cm.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Utility")
    float GetAverageLocationDistance() const;

    /**
     * Calculates the centroid (center of mass) of all locations.
     * @return FVector representing the average position of all locations.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Utility")
    FVector GetLocationCentroid() const;

    /**
     * Prints comprehensive location statistics to the log
     * (counts, average distance, centroid).
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
    FVector ParseCustomCoordinate(const FString& CoordString) const;

    /// Gets player pawn with null check and logging
    APawn* GetPlayerPawn() const;

    /// Finds actor by tag name in the world
    AActor* FindActorWithTag(const FString& Tag) const;

    // ========================================
    // DATA STORAGE
    // ========================================

    /// Master array of all discovered locations
    UPROPERTY()
    TArray<FSpawnLocation> DiscoveredLocations;

    /// Fast lookup map: LocationName → FSpawnLocation
    UPROPERTY()
    TMap<FString, FSpawnLocation> LocationDatabase;

    /// All unique tags found across locations
    UPROPERTY()
    TArray<FString> DiscoveredLocationTags;

    /// Occupancy tracking: LocationName → Occupying Actor
    UPROPERTY()
    TMap<FString, AActor*> LocationOccupancy;

    /// World context for line traces and actor queries
    UPROPERTY()
    UWorld* WorldContext = nullptr;

    /// Flag: Initial scan complete?
    UPROPERTY()
    bool bIsScanComplete = false;

    /// Flag: Scan in progress?
    UPROPERTY()
    bool bIsScanning = false;

    UPROPERTY()
    TMap<FString, FActorArrayWrapper> ActorTagCache; // Need a wrapper struct for TArray in TMap
    
    bool bCacheDirty = true;
public: 
    // ========================================
    // 2.5D FIGHTING GAME FALLBACK SYSTEM
    // ========================================

    /** Generates random corner position for 2.5D fighting game. */
    FVector GetRandomCornerPosition(const FString& CornerType);

    /** Generates random background position (far from camera). */
    FVector GetRandomBackgroundPosition();

    /** Generates random foreground position (close to camera). */
    FVector GetRandomForegroundPosition();

    /** Generates random overhead position (high Z). */
    FVector GetRandomOverheadPosition();

    /** Generates random left side position for 2.5D arena. */
    FVector GetRandomLeftSidePosition();

    /** Generates random right side position for 2.5D arena. */
    FVector GetRandomRightSidePosition();

    /** Generates random center arena position. */
    FVector GetRandomCenterPosition();

    // ========================================
    // DYNAMIC ACTOR TAG CACHING - RUNTIME ACTOR DISCOVERY
    // ========================================

public:
    // ========================================
    // CACHE INITIALIZATION & MANAGEMENT
    // ========================================

    /**
     * Scans entire world and builds actor tag cache for non-"Loc_*" tags.
     * Call during init.
     *
     * @param InWorldContext World to scan.
     * @param bLogResults If true, prints full cache report.
     */
    // UFUNCTION(BlueprintCallable, Category = "LocationEngine|Cache")
    // void ScanAndCacheActorsByTags(
    //     UWorld* InWorldContext,
    //     bool bLogResults = true
    // );

    /**
     * Refreshes the cache for a single tag (incremental update).
     * More efficient than a full rescan.
     *
     * @param Tag Tag name to refresh (e.g., "Enemy", "Item").
     */
    // UFUNCTION(BlueprintCallable, Category = "LocationEngine|Cache")
    // void RefreshTagCache(const FString& Tag);

    /** Clears all cached actor tags. */
    // UFUNCTION(BlueprintCallable, Category = "LocationEngine|Cache")
    // void ClearActorTagCache();

    // ========================================
    // ACTOR QUERY BY TAG - CORE RETRIEVAL FUNCTIONS
    // ========================================

    /**
     * Gets all actors from the cache with a specific tag. O(1) lookup.
     * @param Tag Tag to search for (case-insensitive).
     * @return Array of AActor* pointers. Check IsValid() before use.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<AActor*> GetActorsWithTag(const FString& Tag) const;

    /**
     * Gets cached world positions for all actors with a specific tag. O(1) lookup.
     * @param Tag Tag name (case-insensitive).
     * @return Array of FVector world positions.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FVector> GetPositionsByTag(const FString& Tag) const;

    /**
     * Gets a random actor from the cache for a given tag.
     * @param Tag Tag to search.
     * @return Random actor with tag, or nullptr if tag not found/empty.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    AActor* GetRandomActorWithTag(const FString& Tag);

    /**
     * Gets the cached position of a random actor with the given tag.
     * @param Tag Tag to search.
     * @return Random position, or FVector::ZeroVector if not found.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    FVector GetRandomPositionFromTag(const FString& Tag);

    /**
     * Gets the closest actor from the cache with the given tag,
     * relative to a reference position.
     * @param Tag           Tag to search.
     * @param FromLocation  Reference point (typically player position).
     * @return Closest actor with tag, or nullptr if none found.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    AActor* GetClosestActorWithTag(const FString& Tag, FVector FromLocation);

    /**
     * Gets the farthest actor from the cache with the given tag,
     * relative to a reference position.
     * @param Tag           Tag to search.
     * @param FromLocation  Reference point.
     * @return Farthest actor with tag, or nullptr if none found.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    AActor* GetFarthestActorWithTag(const FString& Tag, FVector FromLocation);
    /**
     * Finds a safe spawn position with clearance and actor spacing validation.
     * Uses intelligent retry logic across multiple fallback strategies.
     * @param MinClearance Minimum sphere clearance required (default 150cm).
     * @param MaxAttempts Maximum number of positions to try (default 10).
     * @return Validated spawn position, or elevated emergency position if all attempts fail.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Validation")
    FVector FindSafeSpawnPosition(float MinClearance = 150.0f, int32 MaxAttempts = 10);

    // ========================================
    // BATCH TAG OPERATIONS
    // ========================================

    /**
     * Gets all unique actor tags currently in the cache.
     * @return Array of discovered tag names (e.g., "Enemy", "Item").
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    TArray<FString> GetAllActorTags() const;

    /**
     * Gets the count of actors from the cache with a specific tag.
     * @param Tag Tag to count.
     * @return Number of actors with tag, 0 if tag not found.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Query")
    int32 GetActorCountWithTag(const FString& Tag) const;

    // ========================================
    // DEBUG & VISUALIZATION
    // ========================================

    /**
     * Prints a complete report of the actor tag cache to the log.
     * Includes tag names, counts, and actor details.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
    void PrintActorTagCache() const;

    /**
     * Visualizes all cached actors in the world with debug geometry.
     * @param Duration How long to display (seconds, default 5.0).
     * @param bCentered If true, also show line to world center.
     * @param Radius Sphere radius for visualization (default 50.0).
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
    void VisualizeActorTagCache(
        float Duration = 5.0f,
        bool bCentered = false,
        float Radius = 50.0f
    );

    // ========================================
    // STATISTICS & ANALYSIS
    // ========================================

    /**
     * Gets the average distance between all actors with a specific tag.
     * Expensive O(n^2) operation.
     * @param Tag Tag to analyze.
     * @return Average distance between actors.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Stats")
    float GetAverageActorDistance(const FString& Tag) const;

    /**
     * Gets the center of mass (centroid) for all actors with a specific tag.
     * @param Tag Tag to analyze.
     * @return Average position of all actors with tag.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Stats")
    FVector GetActorCentroid(const FString& Tag) const;

    /**
     * Prints statistics about actors with a specific tag to the log.
     * Includes count, average spacing, and centroid.
     * @param Tag Tag to analyze.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Stats")
    void PrintActorTagStats(const FString& Tag) const;


private:
    // ========================================
    // TAG-BASED ACTOR QUERIES
    // ========================================

    /// Find random actor with specific tag (real-time, no cache)
    AActor* FindRandomActorWithTag(const FString& Tag);

    /// Find closest actor with specific tag (real-time, no cache)
    AActor* FindClosestActorWithTag(const FString& Tag, FVector FromLocation);
public:
    /**
 * Initializes playable area bounds either from level geometry or manual values.
 * Call this during LocationEngine initialization.
 * @param CustomBounds Optional manual bounds. If not provided, auto-detects from level.
 */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Bounds")
    void InitializePlayableAreaBounds();
    /**
     * Initializes playable area bounds with custom values.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Bounds")
    void InitializePlayableAreaBoundsCustom(FBox CustomBounds);
public:
    /**
     * Returns whether bounds have been initialized.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Bounds")
    bool IsBoundsInitialized() const { return bBoundsInitialized; }
    
    /**
     * Returns the playable area bounding box.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Bounds")
    FBox GetPlayableAreaBounds() const { return PlayableAreaBounds; }

    /**
     * Checks if a position is within the playable area bounds.
     * @param Position World position to test.
     * @return true if position is inside bounds, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Bounds")
    bool IsPositionInBounds(FVector Position) const;

    /**
     * Clamps a position to stay within playable area bounds.
     * @param Position Position to clamp.
     * @return Clamped position guaranteed to be inside bounds.
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Bounds")
    FVector ClampPositionToBounds(FVector Position) const;

    /**
     * Visualizes the playable area bounds with debug geometry.
     * @param Duration How long to display (seconds).
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine|Debug")
    void VisualizePlayableAreaBounds(float Duration = 10.0f) const;
private:
    /**
     * Playable area bounds for 2.5D fighting game.
     * All spawned actors must stay within these limits.
     * Configured at runtime or set to default values.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds", meta = (AllowPrivateAccess = "true"))
    FBox PlayableAreaBounds;
    
    /** Z-height range for ground-level spawns (characters, props) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds", meta = (AllowPrivateAccess = "true"))
    FVector2D GroundHeightRange = FVector2D(80.0f, 150.0f);
    
    /** Z-height range for aerial spawns (particles, overhead objects) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds", meta = (AllowPrivateAccess = "true"))
    FVector2D AerialHeightRange = FVector2D(400.0f, 600.0f);
    
    /** Cached center of playable area for quick access */
    FVector PlayableAreaCenter = FVector::ZeroVector;
    
    /** Flag: Has bounds been initialized? */
    bool bBoundsInitialized = false;
public:
    
    UFUNCTION(Exec, Category = "LocationEngine|Debug")
    void RefreshPlayableAreaBounds() {
        InitializePlayableAreaBounds();
    }

    // In LocationQueryEngine.h
private:
    // ========================================
    // ✅ 2. SPATIAL HASH GRID VARIABLES
    // ========================================
    
    // The optimized grid. 
    // Key = AActor*, Precision = double
    TSharedPtr<UE::Geometry::TPointHashGrid3<AActor*, double>> SpatialGrid;

    // Helper map to track where actors were inserted (needed for fast removal)
    UPROPERTY()
    TMap<AActor*, FVector> OccupiedActorRegistry;

    // Flag to ensure grid is ready
    bool bGridInitialized = false;

    
public:
    /**
     * Configure LLM fallback.
     * @param Endpoint - API URL
     * @param APIKey - API key (empty = local)
     * @param ModelName - Model name
     */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void ConfigureLLMFallback(const FString& Endpoint, const FString& APIKey, const FString& ModelName);

    UFUNCTION(Exec)
    void DebugClearance()
    {
        for (const FSpawnLocation& Loc : DiscoveredLocations)
        {
            if (Loc.bIsOccupied)
            {
                DrawDebugSphere(WorldContext, Loc.WorldPosition, Loc.ClearanceRadius, 12, FColor::Red, false, 10.0f);
            }
        }
    }
private:
    /** LLM resolver (composed) */
    UPROPERTY()
    ULocationResolverLLM* LLMResolver = nullptr;

    /** Build scene context string for LLM */
    FString BuildSceneContext() const;
public:
        /**
         * Finds the best manually placed anchor for a specific tag.
         * @param Tag           The semantic tag (e.g., "Left", "HighGround", "Sniper").
         * @param MinClearance  Radius to check for collision.
         * @return Best anchor location, or FVector::ZeroVector if none found.
         */
        UFUNCTION(BlueprintCallable, Category = "LocationEngine|Anchors")
        FVector GetBestAnchorFor(const FString& Tag, float MinClearance = 150.0f);
public:
    // Add this Getter
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    ULocationResolverLLM* GetLLMResolver() const { return LLMResolver; }

};