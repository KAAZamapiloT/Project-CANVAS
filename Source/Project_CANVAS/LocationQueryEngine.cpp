// Fill out your copyright notice in the Description page of Project Settings.

// Fill out your copyright notice in the Description page of Project Settings.

// Fill out your copyright notice in the Description page of Project Settings.

// Fill out your copyright notice in the Description page of Project Settings.

// Fill out your copyright notice in the Description page of Project Settings.

#include "LocationQueryEngine.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "CollisionQueryParams.h"
#include "WorldCollision.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Components/PrimitiveComponent.h"

// ========================================
// INITIALIZATION
// ========================================

void ULocationQueryEngine::ScanWorldLocationsAsync(UWorld* InWorldContext)
{
    if (!InWorldContext)
    {
        UE_LOG(LogTemp, Error, TEXT("LocationEngine: WorldContext is null!"));
        return;
    }
    
    WorldContext = InWorldContext;
    bIsScanning = true;
    bIsScanComplete = false;
    
    UE_LOG(LogTemp, Display, TEXT("LocationEngine: Starting location scan..."));
    
    // Clear existing data
    DiscoveredLocations.Empty();
    LocationDatabase.Empty();
    DiscoveredLocationTags.Empty();
    
    // Scan for tagged spawn points in level
    ScanForNamedLocations(WorldContext);
    
    // Register default locations (similar to default textures)
    FSpawnLocation CenterLocation;
    CenterLocation.LocationName = TEXT("CENTER");
    CenterLocation.WorldPosition = FVector::ZeroVector;
    CenterLocation.Description = TEXT("World origin");
    CenterLocation.Tags.Add(TEXT("Default"));
    AddLocation(CenterLocation);
    
    bIsScanning = false;
    bIsScanComplete = true;
    
    UE_LOG(LogTemp, Display, TEXT("LocationEngine: Scan complete. Found %d locations."), 
        DiscoveredLocations.Num());
}

void ULocationQueryEngine::ScanForNamedLocations(UWorld* InWorldContext)
{
    if (!InWorldContext) return;
    
    // Find all actors with "Loc_*" tags (similar to how AssetIndexer scans)
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(InWorldContext, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        
        // Check actor tags for "Loc_" prefix
        for (const FName& Tag : Actor->Tags)
        {
            FString TagStr = Tag.ToString();
            if (TagStr.StartsWith(TEXT("Loc_")))
            {
                FSpawnLocation NewLocation;
                NewLocation.LocationName = TagStr.RightChop(4); // Remove "Loc_" prefix
                NewLocation.WorldPosition = Actor->GetActorLocation();
                NewLocation.WorldRotation = Actor->GetActorRotation();
                NewLocation.Description = FString::Printf(TEXT("Named spawn point: %s"), 
                    *NewLocation.LocationName);
                
                // Extract additional tags
                for (const FName& OtherTag : Actor->Tags)
                {
                    FString OtherTagStr = OtherTag.ToString();
                    if (!OtherTagStr.StartsWith(TEXT("Loc_")))
                    {
                        NewLocation.Tags.AddUnique(OtherTagStr);
                        DiscoveredLocationTags.AddUnique(OtherTagStr);
                    }
                }
                
                AddLocation(NewLocation);
                
                UE_LOG(LogTemp, Display, TEXT("LocationEngine: Registered '%s' at %s"), 
                    *NewLocation.LocationName, *NewLocation.WorldPosition.ToString());
                break;
            }
        }
    }
}

// ========================================
// LOCATION DATABASE MANAGEMENT
// ========================================

TArray<FString> ULocationQueryEngine::GetDiscoveredLocationNames() const
{
    TArray<FString> Names;
    LocationDatabase.GetKeys(Names);
    return Names;
}

TArray<FSpawnLocation> ULocationQueryEngine::GetLocationsByTag(const FString& Tag) const
{
    TArray<FSpawnLocation> FilteredLocations;
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        if (Loc.Tags.Contains(Tag))
        {
            FilteredLocations.Add(Loc);
        }
    }
    return FilteredLocations;
}

TArray<FString> ULocationQueryEngine::GetDiscoveredLocationTags() const
{
    return DiscoveredLocationTags;
}

// ========================================
// LOCATION RESOLUTION
// ========================================

FVector ULocationQueryEngine::ResolveLocationName(const FString& LocationName)
{
    FString UpperName = LocationName.ToUpper();
    
    // 1. Check database for named locations
    if (LocationDatabase.Contains(UpperName))
    {
        return LocationDatabase[UpperName].WorldPosition;
    }
    
    // 2. Handle player-relative locations
    if (UpperName.Contains(TEXT("PLAYER")))
    {
        if (UpperName.Contains(TEXT("FRONT"))) return GetPlayerFrontPosition();
        if (UpperName.Contains(TEXT("BACK"))) return GetPlayerBackPosition();
        if (UpperName.Contains(TEXT("LEFT"))) return GetPlayerLeftPosition();
        if (UpperName.Contains(TEXT("RIGHT"))) return GetPlayerRightPosition();
        if (UpperName.Contains(TEXT("POSITION"))) return GetPlayerPosition();
        if (UpperName.Contains(TEXT("NEAR"))) return GetRandomPositionNearPlayer();
    }
    
    // 3. Handle custom coordinates: "CUSTOM:[X,Y,Z]"
    if (UpperName.StartsWith(TEXT("CUSTOM:")))
    {
        return ParseCustomCoordinate(UpperName);
    }
    
    // 4. Handle actor-relative: "NEAR:ActorTag"
    if (UpperName.StartsWith(TEXT("NEAR:")))
    {
        FString ActorTag = UpperName.RightChop(5).TrimStartAndEnd();
        AActor* TargetActor = FindActorWithTag(ActorTag);
        if (TargetActor)
        {
            return TargetActor->GetActorLocation();
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("LocationEngine: Could not resolve '%s', using world origin"), 
        *LocationName);
    return FVector::ZeroVector;
}

FSpawnLocation ULocationQueryEngine::ResolveLocation(const FString& LocationName)
{
    FString UpperName = LocationName.ToUpper();
    
    if (LocationDatabase.Contains(UpperName))
    {
        return LocationDatabase[UpperName];
    }
    
    // Create dynamic location for unregistered names
    FSpawnLocation DynamicLocation;
    DynamicLocation.LocationName = LocationName;
    DynamicLocation.WorldPosition = ResolveLocationName(LocationName);
    DynamicLocation.Description = TEXT("Dynamic location");
    return DynamicLocation;
}

bool ULocationQueryEngine::DoesLocationExist(const FString& LocationName) const
{
    return LocationDatabase.Contains(LocationName.ToUpper());
}

// ========================================
// ADD/REMOVE/MODIFY LOCATIONS
// ========================================

void ULocationQueryEngine::AddLocation(const FSpawnLocation& NewLocation)
{
    FString UpperName = NewLocation.LocationName.ToUpper();
    
    // Add to database
    LocationDatabase.Add(UpperName, NewLocation);
    DiscoveredLocations.Add(NewLocation);
    
    // Update tag list
    for (const FString& Tag : NewLocation.Tags)
    {
        DiscoveredLocationTags.AddUnique(Tag);
    }
    
    UE_LOG(LogTemp, Display, TEXT("LocationEngine: Added location '%s'"), *NewLocation.LocationName);
}

void ULocationQueryEngine::AddLocationByPosition(const FString& LocationName, FVector Position, float Clearance)
{
    FSpawnLocation NewLocation;
    NewLocation.LocationName = LocationName;
    NewLocation.WorldPosition = Position;
    NewLocation.ClearanceRadius = Clearance;
    NewLocation.Description = FString::Printf(TEXT("Custom location: %s"), *LocationName);
    AddLocation(NewLocation);
}

bool ULocationQueryEngine::RemoveLocation(const FString& LocationName)
{
    FString UpperName = LocationName.ToUpper();
    
    if (LocationDatabase.Contains(UpperName))
    {
        LocationDatabase.Remove(UpperName);
        
        // Remove from array
        DiscoveredLocations.RemoveAll([&](const FSpawnLocation& Loc) {
            return Loc.LocationName.Equals(LocationName, ESearchCase::IgnoreCase);
        });
        
        UE_LOG(LogTemp, Display, TEXT("LocationEngine: Removed location '%s'"), *LocationName);
        return true;
    }
    
    return false;
}

bool ULocationQueryEngine::ModifyLocation(const FString& LocationName, const FSpawnLocation& UpdatedLocation)
{
    FString UpperName = LocationName.ToUpper();
    
    if (LocationDatabase.Contains(UpperName))
    {
        LocationDatabase[UpperName] = UpdatedLocation;
        
        // Update in array
        for (FSpawnLocation& Loc : DiscoveredLocations)
        {
            if (Loc.LocationName.Equals(LocationName, ESearchCase::IgnoreCase))
            {
                Loc = UpdatedLocation;
                break;
            }
        }
        
        UE_LOG(LogTemp, Display, TEXT("LocationEngine: Modified location '%s'"), *LocationName);
        return true;
    }
    
    return false;
}

void ULocationQueryEngine::ClearAllLocations()
{
    DiscoveredLocations.Empty();
    LocationDatabase.Empty();
    DiscoveredLocationTags.Empty();
    UE_LOG(LogTemp, Display, TEXT("LocationEngine: Cleared all locations"));
}

// ========================================
// TAG-BASED QUERIES
// ========================================

TArray<FSpawnLocation> ULocationQueryEngine::FindLocationsByTag(const FString& Tag)
{
    return GetLocationsByTag(Tag);
}

void ULocationQueryEngine::AddTagToLocation(const FString& LocationName, const FString& Tag)
{
    FString UpperName = LocationName.ToUpper();
    
    if (LocationDatabase.Contains(UpperName))
    {
        LocationDatabase[UpperName].Tags.AddUnique(Tag);
        DiscoveredLocationTags.AddUnique(Tag);
        
        // Update array
        for (FSpawnLocation& Loc : DiscoveredLocations)
        {
            if (Loc.LocationName.Equals(LocationName, ESearchCase::IgnoreCase))
            {
                Loc.Tags.AddUnique(Tag);
                break;
            }
        }
    }
}

void ULocationQueryEngine::RemoveTagFromLocation(const FString& LocationName, const FString& Tag)
{
    FString UpperName = LocationName.ToUpper();
    
    if (LocationDatabase.Contains(UpperName))
    {
        LocationDatabase[UpperName].Tags.Remove(Tag);
        
        // Update array
        for (FSpawnLocation& Loc : DiscoveredLocations)
        {
            if (Loc.LocationName.Equals(LocationName, ESearchCase::IgnoreCase))
            {
                Loc.Tags.Remove(Tag);
                break;
            }
        }
    }
}

// ========================================
// PLAYER-RELATIVE QUERIES
// ========================================

FVector ULocationQueryEngine::GetPlayerFrontPosition(float Distance)
{
    APawn* PlayerPawn = GetPlayerPawn();
    if (PlayerPawn)
    {
        return PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector() * Distance;
    }
    return FVector::ZeroVector;
}

FVector ULocationQueryEngine::GetPlayerBackPosition(float Distance)
{
    APawn* PlayerPawn = GetPlayerPawn();
    if (PlayerPawn)
    {
        return PlayerPawn->GetActorLocation() - PlayerPawn->GetActorForwardVector() * Distance;
    }
    return FVector::ZeroVector;
}

FVector ULocationQueryEngine::GetPlayerLeftPosition(float Distance)
{
    APawn* PlayerPawn = GetPlayerPawn();
    if (PlayerPawn)
    {
        return PlayerPawn->GetActorLocation() - PlayerPawn->GetActorRightVector() * Distance;
    }
    return FVector::ZeroVector;
}

FVector ULocationQueryEngine::GetPlayerRightPosition(float Distance)
{
    APawn* PlayerPawn = GetPlayerPawn();
    if (PlayerPawn)
    {
        return PlayerPawn->GetActorLocation() + PlayerPawn->GetActorRightVector() * Distance;
    }
    return FVector::ZeroVector;
}

FVector ULocationQueryEngine::GetPlayerPosition()
{
    APawn* PlayerPawn = GetPlayerPawn();
    return PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
}

FVector ULocationQueryEngine::GetRandomPositionNearPlayer(float MinDistance, float MaxDistance)
{
    FVector PlayerPos = GetPlayerPosition();
    float Angle = FMath::RandRange(0.0f, 360.0f);
    float Distance = FMath::RandRange(MinDistance, MaxDistance);
    
    return PlayerPos + FVector(
        FMath::Cos(FMath::DegreesToRadians(Angle)) * Distance,
        FMath::Sin(FMath::DegreesToRadians(Angle)) * Distance,
        0.0f
    );
}

// ========================================
// OCCUPANCY MANAGEMENT
// ========================================

void ULocationQueryEngine::SetLocationOccupied(const FString& LocationName, bool bOccupied, AActor* OccupyingActor)
{
    FString UpperName = LocationName.ToUpper();
    
    if (LocationDatabase.Contains(UpperName))
    {
        LocationDatabase[UpperName].bIsOccupied = bOccupied;
        LocationDatabase[UpperName].OccupyingActor = OccupyingActor;
        
        // Update array
        for (FSpawnLocation& Loc : DiscoveredLocations)
        {
            if (Loc.LocationName.Equals(LocationName, ESearchCase::IgnoreCase))
            {
                Loc.bIsOccupied = bOccupied;
                Loc.OccupyingActor = OccupyingActor;
                break;
            }
        }
    }
}

bool ULocationQueryEngine::IsLocationOccupied(const FString& LocationName) const
{
    FString UpperName = LocationName.ToUpper();
    
    if (LocationDatabase.Contains(UpperName))
    {
        return LocationDatabase[UpperName].bIsOccupied;
    }
    
    return false;
}

TArray<FSpawnLocation> ULocationQueryEngine::GetFreeLocations() const
{
    TArray<FSpawnLocation> FreeLocations;
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        if (!Loc.bIsOccupied)
        {
            FreeLocations.Add(Loc);
        }
    }
    return FreeLocations;
}

void ULocationQueryEngine::ClearAllOccupancy()
{
    for (FSpawnLocation& Loc : DiscoveredLocations)
    {
        Loc.bIsOccupied = false;
        Loc.OccupyingActor = nullptr;
    }
    
    for (auto& Pair : LocationDatabase)
    {
        Pair.Value.bIsOccupied = false;
        Pair.Value.OccupyingActor = nullptr;
    }
}

// ========================================
// COLLISION & VALIDATION
// ========================================

FSpawnLocation ULocationQueryEngine::FindNearestFreeLocation(FVector PreferredLocation, float MinClearance)
{
    // Check if preferred location is clear
    if (IsLocationClear(PreferredLocation, MinClearance))
    {
        FSpawnLocation Result;
        Result.LocationName = TEXT("Dynamic");
        Result.WorldPosition = PreferredLocation;
        Result.ClearanceRadius = MinClearance;
        return Result;
    }
    
    // Search in concentric rings
    const float SearchStep = 250.0f;
    const int32 MaxRings = 5;
    const int32 SamplesPerRing = 12;
    
    for (int32 Ring = 1; Ring <= MaxRings; Ring++)
    {
        for (int32 Sample = 0; Sample < SamplesPerRing; Sample++)
        {
            float Angle = (360.0f / SamplesPerRing) * Sample;
            float Radian = FMath::DegreesToRadians(Angle);
            float Distance = SearchStep * Ring;
            
            FVector TestLocation = PreferredLocation + FVector(
                FMath::Cos(Radian) * Distance,
                FMath::Sin(Radian) * Distance,
                0.0f
            );
            
            if (IsLocationClear(TestLocation, MinClearance))
            {
                FSpawnLocation Result;
                Result.LocationName = TEXT("Dynamic");
                Result.WorldPosition = TestLocation;
                Result.ClearanceRadius = MinClearance;
                return Result;
            }
        }
    }
    
    // Return preferred location if nothing found
    FSpawnLocation Result;
    Result.LocationName = TEXT("Dynamic");
    Result.WorldPosition = PreferredLocation;
    Result.ClearanceRadius = MinClearance;
    return Result;
}

bool ULocationQueryEngine::IsLocationClear(FVector Location, float Radius) const
{
    if (!WorldContext) return false;
    
    // Use sphere overlap test without FOverlapResult
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);
    
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    
    // Use the simpler API that returns bool directly
    bool bHasBlockingHit = WorldContext->OverlapAnyTestByChannel(
        Location,
        FQuat::Identity,
        ECC_WorldStatic,
        SphereShape,
        QueryParams
    );
    
    // Return true if location is clear (no blocking hit)
    return !bHasBlockingHit;
}


FVector ULocationQueryEngine::SnapToGround(FVector Location, float MaxTraceDistance)
{
    if (!WorldContext) return Location;
    
    FVector Start = Location + FVector(0, 0, 100.0f);
    FVector End = Location - FVector(0, 0, MaxTraceDistance);
    
    FHitResult Hit;
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    
    if (WorldContext->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QueryParams))
    {
        return Hit.Location;
    }
    
    return Location;
}

// ========================================
// LLM INTEGRATION
// ========================================

FString ULocationQueryEngine::GetLocationContextForLLM() const
{
    FString Context = TEXT("=== AVAILABLE SPAWN LOCATIONS ===\n\n");
    
    // Named locations
    if (DiscoveredLocations.Num() > 0)
    {
        Context += TEXT("[Named Locations]\n");
        for (const FSpawnLocation& Loc : DiscoveredLocations)
        {
            FString OccupiedStatus = Loc.bIsOccupied ? TEXT(" [OCCUPIED]") : TEXT("");
            Context += FString::Printf(TEXT("  - \"%s\": %s%s\n"), 
                *Loc.LocationName, *Loc.Description, *OccupiedStatus);
        }
        Context += TEXT("\n");
    }
    
    // Player-relative options
    Context += TEXT("[Player-Relative]\n");
    Context += TEXT("  - \"PLAYER_FRONT\", \"PLAYER_BACK\", \"PLAYER_LEFT\", \"PLAYER_RIGHT\"\n");
    Context += TEXT("  - \"PLAYER_POSITION\", \"NEAR_PLAYER\"\n\n");
    
    // Custom coordinates
    Context += TEXT("[Custom Coordinates]\n");
    Context += TEXT("  - Format: \"CUSTOM:[X,Y,Z]\"\n\n");
    
    return Context;
}

TMap<FString, FSpawnLocation> ULocationQueryEngine::BuildLocationDatabase()
{
    return LocationDatabase;
}

// ========================================
// INTERNAL HELPERS
// ========================================

FVector ULocationQueryEngine::ParseCustomCoordinate(const FString& CoordString) const
{
    FString CleanString = CoordString.RightChop(7).TrimStartAndEnd();
    CleanString.RemoveFromStart(TEXT("["));
    CleanString.RemoveFromEnd(TEXT("]"));
    
    TArray<FString> Coords;
    CleanString.ParseIntoArray(Coords, TEXT(","));
    
    if (Coords.Num() == 3)
    {
        return FVector(
            FCString::Atof(*Coords[0]),
            FCString::Atof(*Coords[1]),
            FCString::Atof(*Coords[2])
        );
    }
    
    return FVector::ZeroVector;
}

APawn* ULocationQueryEngine::GetPlayerPawn() const
{
    if (!WorldContext) return nullptr;
    
    APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
    return PC ? PC->GetPawn() : nullptr;
}

AActor* ULocationQueryEngine::FindActorWithTag(const FString& Tag) const
{
    if (!WorldContext || Tag.IsEmpty())
    {
        return nullptr;
    }
    
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(WorldContext, FName(*Tag), FoundActors);
    
    // Return first valid actor
    for (AActor* Actor : FoundActors)
    {
        if (IsValid(Actor))
        {
            return Actor;
        }
    }
    
    return nullptr;
}

