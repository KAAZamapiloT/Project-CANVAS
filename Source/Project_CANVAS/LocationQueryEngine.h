// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"        // ADD THIS
#include "GameFramework/Actor.h"       // ADD THIS
#include "LocationQueryEngine.generated.h"


/**
 * FSpawnLocation - Represents a semantic spawn location in the world
 * Similar to how FTextureSet represents a collection of textures
 */
USTRUCT(BlueprintType)
struct FSpawnLocation
{
    GENERATED_BODY()
    
    /** Unique semantic name (e.g., "PlayerFront", "Arena_Center") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LocationName;
    
    /** World coordinates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldPosition = FVector::ZeroVector;
    
    /** World rotation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator WorldRotation = FRotator::ZeroRotator;
    
    /** Required clearance radius */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ClearanceRadius = 200.0f;
    
    /** Is this location currently occupied? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsOccupied = false;
    
    /** Tags for filtering (similar to actor tags) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Tags;
    
    /** Description for LLM context */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;
    
    /** Actor currently occupying this location */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TWeakObjectPtr<AActor> OccupyingActor;
};

/**
 * ULocationQueryEngine
 * Manages spawn locations and provides location-aware spawning
 * Similar to AssetIndexer but for locations instead of assets
 */
UCLASS()
class PROJECT_CANVAS_API ULocationQueryEngine : public UObject
{
    GENERATED_BODY()
    
public:
    // ========================================
    // INITIALIZATION (Similar to AssetIndexer::ScanAllAssetsAsync)
    // ========================================
    
    /** Scan world for tagged spawn points and build location database */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void ScanWorldLocationsAsync(UWorld* WorldContext);
    
    /** Check if location scan is complete */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    bool IsScanComplete() const { return bIsScanComplete; }
    
    // ========================================
    // LOCATION DATABASE MANAGEMENT (Similar to AssetIndexer getters)
    // ========================================
    
    /** Get all discovered location names */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    TArray<FString> GetDiscoveredLocationNames() const;
    
    /** Get all locations */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    TArray<FSpawnLocation> GetAllLocations() const { return DiscoveredLocations; }
    
    /** Get locations with specific tag */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    TArray<FSpawnLocation> GetLocationsByTag(const FString& Tag) const;
    
    /** Get all location tags (similar to GetDiscoveredActorTags) */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    TArray<FString> GetDiscoveredLocationTags() const;
    
    // ========================================
    // LOCATION RESOLUTION (Similar to AssetIndexer::ResolveTextureFromName)
    // ========================================
    
    /** Resolve semantic location name to world position */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FVector ResolveLocationName(const FString& LocationName);
    
    /** Resolve location name to full FSpawnLocation struct */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FSpawnLocation ResolveLocation(const FString& LocationName);
    
    /** Check if location name exists */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    bool DoesLocationExist(const FString& LocationName) const;
    
    // ========================================
    // ADD/REMOVE/MODIFY LOCATIONS (Runtime Management)
    // ========================================
    
    /** Add a new location to the database */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void AddLocation(const FSpawnLocation& NewLocation);
    
    /** Add location by position and name */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void AddLocationByPosition(const FString& LocationName, FVector Position, float Clearance = 200.0f);
    
    /** Remove location by name */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    bool RemoveLocation(const FString& LocationName);
    
    /** Modify existing location */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    bool ModifyLocation(const FString& LocationName, const FSpawnLocation& UpdatedLocation);
    
    /** Clear all locations */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void ClearAllLocations();
    
    // ========================================
    // TAG-BASED QUERIES (Similar to actor tag system)
    // ========================================
    
    /** Find all locations with specific tag at runtime */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    TArray<FSpawnLocation> FindLocationsByTag(const FString& Tag);
    
    /** Add tag to existing location */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void AddTagToLocation(const FString& LocationName, const FString& Tag);
    
    /** Remove tag from location */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void RemoveTagFromLocation(const FString& LocationName, const FString& Tag);
    
    // ========================================
    // PLAYER-RELATIVE QUERIES
    // ========================================
    
    /** Get position in front of player */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FVector GetPlayerFrontPosition(float Distance = 300.0f);
    
    /** Get position behind player */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FVector GetPlayerBackPosition(float Distance = 300.0f);
    
    /** Get position left of player */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FVector GetPlayerLeftPosition(float Distance = 200.0f);
    
    /** Get position right of player */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FVector GetPlayerRightPosition(float Distance = 200.0f);
    
    /** Get player's current position */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FVector GetPlayerPosition();
    
    /** Get random position near player */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FVector GetRandomPositionNearPlayer(float MinDistance = 200.0f, float MaxDistance = 500.0f);
    
    // ========================================
    // OCCUPANCY MANAGEMENT (Similar to scene state tracking)
    // ========================================
    
    /** Mark location as occupied */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void SetLocationOccupied(const FString& LocationName, bool bOccupied, AActor* OccupyingActor = nullptr);
    
    /** Check if location is occupied */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    bool IsLocationOccupied(const FString& LocationName) const;
    
    /** Get all free (unoccupied) locations */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    TArray<FSpawnLocation> GetFreeLocations() const;
    
    /** Clear all occupancy markers */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    void ClearAllOccupancy();
    
    // ========================================
    // COLLISION & VALIDATION
    // ========================================
    
    /** Find nearest free location from a preferred position */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FSpawnLocation FindNearestFreeLocation(FVector PreferredLocation, float MinClearance = 150.0f);
    
    /** Check if location has sufficient clearance */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    bool IsLocationClear(FVector Location, float Radius) const;
    
    /** Snap location to ground */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FVector SnapToGround(FVector Location, float MaxTraceDistance = 1000.0f);
    
    // ========================================
    // LLM INTEGRATION (Similar to AssetIndexer context building)
    // ========================================
    
    /** Get formatted location context string for LLM prompt (like GetDiscoveredTextureNames) */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    FString GetLocationContextForLLM() const;
    
    /** Build location database for prompt generation */
    UFUNCTION(BlueprintCallable, Category = "LocationEngine")
    TMap<FString, FSpawnLocation> BuildLocationDatabase();
    
private:
    // ========================================
    // INTERNAL HELPERS
    // ========================================
    
    /** Scan for actors tagged with "Loc_*" (similar to texture scanning) */
    void ScanForNamedLocations(UWorld* WorldContext);
    
    /** Parse custom coordinate string: "CUSTOM:[X,Y,Z]" */
    FVector ParseCustomCoordinate(const FString& CoordString) const;
    
    /** Get player pawn safely */
    APawn* GetPlayerPawn() const;
    
    /** Find actor by tag */
    AActor* FindActorWithTag(const FString& Tag) const;
    
    // ========================================
    // DATA STORAGE (Similar to AssetIndexer storage)
    // ========================================
    
    /** All discovered locations */
    UPROPERTY()
    TArray<FSpawnLocation> DiscoveredLocations;
    
    /** Quick lookup map: LocationName → FSpawnLocation */
    UPROPERTY()
    TMap<FString, FSpawnLocation> LocationDatabase;
    
    /** All unique tags found on locations */
    UPROPERTY()
    TArray<FString> DiscoveredLocationTags;
    
    /** World context reference */
    UPROPERTY()
    UWorld* WorldContext;
    
    /** Is scanning complete? */
    UPROPERTY()
    bool bIsScanComplete = false;
    
    /** Is scanning in progress? */
    UPROPERTY()
    bool bIsScanning = false;
};
