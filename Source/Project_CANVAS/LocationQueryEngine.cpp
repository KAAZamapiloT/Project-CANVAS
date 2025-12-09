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
#include "Engine/OverlapResult.h"  // Add this line
#include "ScenePlan.h"  // Add at top
#include "GameFramework/PlayerStart.h"
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
   
    // Register default locations
    FSpawnLocation CenterLocation;
    CenterLocation.LocationName = TEXT("CENTER");
    CenterLocation.WorldPosition = FVector::ZeroVector;
    CenterLocation.Description = TEXT("World origin");
    CenterLocation.Tags.Add(TEXT("Default"));
    AddLocation(CenterLocation);


    SpatialGrid = MakeShared<UE::Geometry::TPointHashGrid3<AActor*, double>>(500.0, nullptr);
    OccupiedActorRegistry.Empty();
    bGridInitialized = true;

    UE_LOG(LogTemp, Display, TEXT("✅ LocationEngine: Spatial Hash Grid Initialized"));
    
    bIsScanning = false;
    bIsScanComplete = true;
    // ✅ ADD THIS: Initialize LLM resolver
    if (!LLMResolver)
    {
        LLMResolver = NewObject<ULocationResolverLLM>(this);
        UE_LOG(LogTemp, Display, TEXT("✅ LocationEngine: LLM resolver created (not configured)"));
    }
    UE_LOG(LogTemp, Display, TEXT("LocationEngine: Scan complete. Found %d locations."), 
        DiscoveredLocations.Num());
}

void ULocationQueryEngine::ScanForNamedLocations(UWorld* InWorldContext)
{
    if (!InWorldContext) return;
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(InWorldContext, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (!Actor) continue;
        
        for (const FName& Tag : Actor->Tags)
        {
            FString TagStr = Tag.ToString();
            if (TagStr.StartsWith(TEXT("Loc_")))
            {
                FSpawnLocation NewLocation;
                NewLocation.LocationName = TagStr.RightChop(4);
                NewLocation.WorldPosition = Actor->GetActorLocation();
                NewLocation.WorldRotation = Actor->GetActorRotation();
                NewLocation.Description = FString::Printf(TEXT("Named spawn point: %s"), 
                    *NewLocation.LocationName);
                
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
// BOUNDING BOX

// LocationQueryEngine.cpp

void ULocationQueryEngine::InitializePlayableAreaBounds()
{
    if (!WorldContext) return;

    UE_LOG(LogTemp, Warning, TEXT("🔍 LocationEngine: Initializing Bounds (Tag-Based Mode)..."));

    // 1. Reset Bounds
    PlayableAreaBounds = FBox(EForceInit::ForceInit); // Starts invalid/inverted

    // 2. Define the tags that create the arena
    TArray<FName> BoundaryTags = {
        FName("Ground.Floor"),
        FName("Aerial.Ceiling"),
        FName("Location.Center"), 
        FName("Background.Wall"), 
        FName("Side.Wall"),
        FName("Arena.Bounds") // Add this tag to your distant corner markers
    };

    int32 FoundAnchors = 0;

    // 3. Iterate Tags
    for (const FName& Tag : BoundaryTags)
    {
        TArray<AActor*> TaggedActors;
        UGameplayStatics::GetAllActorsWithTag(WorldContext, Tag, TaggedActors);

        for (AActor* Actor : TaggedActors)
        {
            if (!IsValid(Actor)) continue;

            FVector Origin, BoxExtent;
            // false = include visual meshes (even if Hidden or NoCollision)
            Actor->GetActorBounds(false, Origin, BoxExtent);

            // --- LOGIC SPLIT ---
            
            // CASE A: It is a Mesh/Volume (Has Size)
            if (BoxExtent.SizeSquared() > 1.0f) 
            {
                PlayableAreaBounds += FBox(Origin - BoxExtent, Origin + BoxExtent);
                FoundAnchors++;
            }
            // CASE B: It is a Marker/Empty Actor (No Size)
            // We MUST include its location, otherwise markers are ignored!
            else 
            {
                FVector Loc = Actor->GetActorLocation();
                PlayableAreaBounds += Loc; // Expand box to include this point
                FoundAnchors++;
                UE_LOG(LogTemp, Display, TEXT("   -> Included Marker: %s (at %s)"), *Actor->GetName(), *Loc.ToString());
            }
        }
    }

    // 4. FALLBACK: If NO tags found, assume Player is center
    if (FoundAnchors == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No tagged geometry found. Using Player-Centric Fallback."));
        
        FVector Center = FVector::ZeroVector;
        APawn* Player = UGameplayStatics::GetPlayerPawn(WorldContext, 0);
        if (Player) Center = Player->GetActorLocation();
        else 
        {
             AActor* Start = UGameplayStatics::GetActorOfClass(WorldContext, APlayerStart::StaticClass());
             if (Start) Center = Start->GetActorLocation();
        }

        // Default 40m x 16m Arena
        FVector Extent(2000.0f, 800.0f, 0.0f); 
        PlayableAreaBounds += (Center - Extent);
        PlayableAreaBounds += (Center + Extent);
    }

    // 5. ENFORCE MINIMUM VOLUME (The 2.5D Fix)
    // Prevent flat boxes if the user only tagged the floor
    FVector Size = PlayableAreaBounds.GetSize();
    FVector Center = PlayableAreaBounds.GetCenter();

    // Ensure X (Depth) >= 10m
    if (Size.X < 1000.0f) 
    {
        PlayableAreaBounds.Min.X = Center.X - 1000.0f;
        PlayableAreaBounds.Max.X = Center.X + 1000.0f;
    }
    // Ensure Y (Width) >= 5m
    if (Size.Y < 500.0f) 
    {
        PlayableAreaBounds.Min.Y = Center.Y - 500.0f;
        PlayableAreaBounds.Max.Y = Center.Y + 500.0f;
    }
    // Ensure Z (Height) >= 8m (Crucial for spawning)
    if (Size.Z < 100.0f) 
    {
        PlayableAreaBounds.Min.Z = Center.Z - 20.0f;  // Slightly below center
        PlayableAreaBounds.Max.Z = Center.Z + 800.0f; // High ceiling
    }

    // 6. Finalize
    bBoundsInitialized = true;
    PlayableAreaCenter = PlayableAreaBounds.GetCenter();

    // Set intelligent height ranges
    GroundHeightRange.X = PlayableAreaBounds.Min.Z;
    GroundHeightRange.Y = PlayableAreaBounds.Min.Z + 200.0f;
    AerialHeightRange.X = PlayableAreaBounds.Min.Z + 400.0f;
    AerialHeightRange.Y = PlayableAreaBounds.Max.Z;

    UE_LOG(LogTemp, Warning, TEXT("✅ Bounds Initialized: %s (Anchors: %d)"), 
        *PlayableAreaBounds.ToString(), FoundAnchors);
        
    VisualizePlayableAreaBounds(15.0f);
}
// Version 2: With custom bounds parameter
void ULocationQueryEngine::InitializePlayableAreaBoundsCustom(FBox CustomBounds)
{
    if (CustomBounds.IsValid && CustomBounds.GetSize().Size() > 0.0f)
    {
        PlayableAreaBounds = CustomBounds;
        bBoundsInitialized = true;
        PlayableAreaCenter = PlayableAreaBounds.GetCenter();
        
        UE_LOG(LogTemp, Display, TEXT("✅ Custom bounds set: %s"), *PlayableAreaBounds.ToString());
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Invalid custom bounds, using auto-detect"));
        InitializePlayableAreaBounds(); // Fall back to auto-detect
    }
}

bool ULocationQueryEngine::IsPositionInBounds(FVector Position) const
{
    if (!bBoundsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ Bounds not initialized, assuming position valid"));
        return true; // Graceful degradation
    }
    
    return PlayableAreaBounds.IsInside(Position);
}

FVector ULocationQueryEngine::ClampPositionToBounds(FVector Position) const
{
    if (!bBoundsInitialized) return Position;
    
    FVector Clamped = Position;
    Clamped.X = FMath::Clamp(Clamped.X, PlayableAreaBounds.Min.X, PlayableAreaBounds.Max.X);
    Clamped.Y = FMath::Clamp(Clamped.Y, PlayableAreaBounds.Min.Y, PlayableAreaBounds.Max.Y);
    Clamped.Z = FMath::Clamp(Clamped.Z, PlayableAreaBounds.Min.Z, PlayableAreaBounds.Max.Z);
    
    return Clamped;
}

void ULocationQueryEngine::VisualizePlayableAreaBounds(float Duration) const
{
    if (!WorldContext || !bBoundsInitialized) return;
    
    // Draw full bounding box
    DrawDebugBox(WorldContext, PlayableAreaCenter, PlayableAreaBounds.GetExtent(), 
        FColor::Cyan, false, Duration, 0, 5.0f);
    
    // Draw center point
    DrawDebugSphere(WorldContext, PlayableAreaCenter, 50.0f, 12, FColor::Yellow, false, Duration);
    
    // Draw ground height range plane
    FVector GroundMin = FVector(PlayableAreaBounds.Min.X, PlayableAreaBounds.Min.Y, GroundHeightRange.X);
    FVector GroundMax = FVector(PlayableAreaBounds.Max.X, PlayableAreaBounds.Max.Y, GroundHeightRange.Y);
    DrawDebugBox(WorldContext, (GroundMin + GroundMax) * 0.5f, (GroundMax - GroundMin) * 0.5f, 
        FColor::Green, false, Duration, 0, 2.0f);
    
    // ✅ NEW: Draw aerial height range plane
    FVector AerialMin = FVector(PlayableAreaBounds.Min.X, PlayableAreaBounds.Min.Y, AerialHeightRange.X);
    FVector AerialMax = FVector(PlayableAreaBounds.Max.X, PlayableAreaBounds.Max.Y, AerialHeightRange.Y);
    DrawDebugBox(WorldContext, (AerialMin + AerialMax) * 0.5f, (AerialMax - AerialMin) * 0.5f, 
        FColor::Blue, false, Duration, 0, 2.0f);
    
    UE_LOG(LogTemp, Display, TEXT("📦 Visualizing playable bounds for %.1f seconds"), Duration);
    UE_LOG(LogTemp, Display, TEXT("   🟢 Green = Ground Zone (%.0f - %.0f)"), GroundHeightRange.X, GroundHeightRange.Y);
    UE_LOG(LogTemp, Display, TEXT("   🔵 Blue = Aerial Zone (%.0f - %.0f)"), AerialHeightRange.X, AerialHeightRange.Y);
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
// LLM INTEGRATION & CONTEXT
// ========================================

TMap<FString, FSpawnLocation> ULocationQueryEngine::BuildLocationDatabase()
{
    TMap<FString, FSpawnLocation> Result;
    
    if (DiscoveredLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ BuildLocationDatabase: No locations discovered"));
        return Result;
    }
    
    // Build map from array
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        FString UpperName = Loc.LocationName.ToUpper();
        Result.Add(UpperName, Loc);
    }
    
    UE_LOG(LogTemp, Display, TEXT("✅ BuildLocationDatabase: Built %d locations"), Result.Num());
    
    return Result;
}

void ULocationQueryEngine::AddLocation(const FSpawnLocation& NewLocation)
{
    FString UpperName = NewLocation.LocationName.ToUpper();
    
    LocationDatabase.Add(UpperName, NewLocation);
    DiscoveredLocations.Add(NewLocation);
    
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
// LOCATION RESOLUTION - ENHANCED WITH ACTOR TAG SUPPORT
// ========================================
FVector ULocationQueryEngine::ResolveLocationName(const FString& LocationName)
{
    FString UpperName = LocationName.ToUpper();
    
    UE_LOG(LogTemp, Display, TEXT("🔍 ResolveLocationName: '%s'"), *LocationName);
    
    // ========================================
    // 1. CHECK DATABASE FOR NAMED LOCATIONS
    // ========================================
    if (LocationDatabase.Contains(UpperName))
    {
        FSpawnLocation& Location = LocationDatabase[UpperName];
        FVector Pos = Location.WorldPosition;
    
        // Check occupancy before returning
        if (Location.bIsOccupied)
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Location '%s' is OCCUPIED - finding alternative..."), *LocationName);
        
            // Try to find free location with same tag
            TArray<FString> Tags = Location.Tags;
            for (const FString& Tag : Tags)
            {
                TArray<FSpawnLocation> TaggedLocs = GetLocationsByTag(Tag);
                for (const FSpawnLocation& Loc : TaggedLocs)
                {
                    if (!Loc.bIsOccupied && IsLocationClear(Loc.WorldPosition, Loc.ClearanceRadius))
                    {
                        UE_LOG(LogTemp, Display, TEXT("✅ Found free alternative: %s"), *Loc.LocationName);
                        return Loc.WorldPosition;
                    }
                }
            }
        
            // If no alternative, fall through to other strategies
            UE_LOG(LogTemp, Warning, TEXT("No free alternatives for '%s', trying other strategies"), *LocationName);
        }
        else
        {
            UE_LOG(LogTemp, Display, TEXT("✅ Found in database: (%.0f, %.0f, %.0f)"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
    }

    // ========================================
    // 2. HANDLE PLAYER-RELATIVE LOCATIONS
    // ========================================
    if (UpperName.Contains(TEXT("PLAYER")))
    {
        if (UpperName.Contains(TEXT("FRONT"))) 
        {
            FVector Pos = GetPlayerFrontPosition();
            UE_LOG(LogTemp, Display, TEXT("   ✅ Player front: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("BACK"))) 
        {
            FVector Pos = GetPlayerBackPosition();
            UE_LOG(LogTemp, Display, TEXT("   ✅ Player back: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("LEFT"))) 
        {
            FVector Pos = GetPlayerLeftPosition();
            UE_LOG(LogTemp, Display, TEXT("   ✅ Player left: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("RIGHT"))) 
        {
            FVector Pos = GetPlayerRightPosition();
            UE_LOG(LogTemp, Display, TEXT("   ✅ Player right: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("POSITION"))) 
        {
            FVector Pos = GetPlayerPosition();
            UE_LOG(LogTemp, Display, TEXT("   ✅ Player position: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("NEAR"))) 
        {
            FVector Pos = GetRandomPositionNearPlayer();
            UE_LOG(LogTemp, Display, TEXT("   ✅ Near player: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
    }
    
    // ========================================
    // 3. HANDLE ENEMY-RELATIVE LOCATIONS
    // ========================================
    if (UpperName.Contains(TEXT("ENEMY")))
    {
        AActor* EnemyActor = FindActorWithTag(TEXT("Enemy.Character"));

        if (!EnemyActor)
        {
            UE_LOG(LogTemp, Warning, TEXT("   ⚠️ Enemy actor not found, using background fallback"));
            return GetRandomBackgroundPosition();
        }

        FVector EnemyPos = EnemyActor->GetActorLocation();
        FVector EnemyForward = EnemyActor->GetActorForwardVector();
        FVector EnemyRight = EnemyActor->GetActorRightVector();

        if (UpperName.Contains(TEXT("FRONT")))
        {
            FVector Pos = EnemyPos + EnemyForward * 300.0f;
            UE_LOG(LogTemp, Display, TEXT("   ✅ Enemy front: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("BACK")))
        {
            FVector Pos = EnemyPos - EnemyForward * 300.0f;
            UE_LOG(LogTemp, Display, TEXT("   ✅ Enemy back: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("LEFT")))
        {
            FVector Pos = EnemyPos - EnemyRight * 300.0f;
            UE_LOG(LogTemp, Display, TEXT("   ✅ Enemy left: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("RIGHT")))
        {
            FVector Pos = EnemyPos + EnemyRight * 300.0f;
            UE_LOG(LogTemp, Display, TEXT("   ✅ Enemy right: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
            return Pos;
        }
        if (UpperName.Contains(TEXT("POSITION")))
        {
            UE_LOG(LogTemp, Display, TEXT("   ✅ Enemy position: [%.0f, %.0f, %.0f]"), EnemyPos.X, EnemyPos.Y, EnemyPos.Z);
            return EnemyPos;
        }
    }
    
    // ========================================
    // 4. HANDLE CUSTOM COORDINATES
    // ========================================
    if (UpperName.StartsWith(TEXT("CUSTOM:")))
    {
        FVector Pos = ParseCustomCoordinate(UpperName);
        UE_LOG(LogTemp, Display, TEXT("   ✅ Parsed custom: [%.0f, %.0f, %.0f]"), Pos.X, Pos.Y, Pos.Z);
        return Pos;
    }
    
    // ========================================
    // 5. HANDLE CLOSEST:Tag
    // ========================================
    if (UpperName.StartsWith(TEXT("CLOSEST:")))
    {
        FString ActorTag = UpperName.RightChop(8).TrimStartAndEnd();
        FVector PlayerPos = GetPlayerPosition();
        
        AActor* ClosestActor = GetClosestActorWithTag(ActorTag, PlayerPos);
        
        if (ClosestActor)
        {
            FVector ActorLocation = ClosestActor->GetActorLocation();
            
            FVector Origin, BoxExtent;
            ClosestActor->GetActorBounds(false, Origin, BoxExtent);
            
            float OffsetDistance = BoxExtent.Size() + 150.0f;
            FVector TowardsPlayer = (PlayerPos - ActorLocation).GetSafeNormal();
            FVector NearPosition = ActorLocation + TowardsPlayer * OffsetDistance;
            
            UE_LOG(LogTemp, Display, TEXT("   ✅ Closest '%s' found"), *ActorTag);
            return NearPosition;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ No actors with tag '%s' found"), *ActorTag);
    }
    
    // ========================================
    // 6. HANDLE NEAR:ActorTag
    // ========================================
    if (UpperName.StartsWith(TEXT("NEAR:")))
    {
        FString ActorTag = UpperName.RightChop(5).TrimStartAndEnd();
        TArray<AActor*> FoundActors = GetActorsWithTag(ActorTag);
        
        if (FoundActors.Num() > 0)
        {
            int32 RandomIndex = FMath::RandRange(0, FoundActors.Num() - 1);
            AActor* TargetActor = FoundActors[RandomIndex];
            
            if (IsValid(TargetActor))
            {
                FVector ActorLocation = TargetActor->GetActorLocation();
                FVector Origin, BoxExtent;
                TargetActor->GetActorBounds(false, Origin, BoxExtent);
                
                float OffsetDistance = BoxExtent.Size() + 150.0f;
                FVector RandomOffset = FVector(
                    FMath::RandRange(-OffsetDistance, OffsetDistance),
                    FMath::RandRange(-OffsetDistance, OffsetDistance),
                    0.0f
                );
                
                FVector NearPosition = ActorLocation + RandomOffset;
                UE_LOG(LogTemp, Display, TEXT("   ✅ Found actor '%s', spawning NEAR"), *ActorTag);
                return NearPosition;
            }
        }
        
        UE_LOG(LogTemp, Warning, TEXT("   ⚠️ Actor tag '%s' not found"), *ActorTag);
    }
    
    // ========================================
    // 7. LLM FALLBACK (BEFORE 2.5D!)
    // ========================================
    /*
     *DISBALING LLM FALLBACK FOR THIS FUNCTION SINCE IT IS CAUSING A LOT OF PROBLEMS AND EACH RESOLUTION CAUSE ONE REQUEST AND
     *IT OVERWHELM THE SERVER
    if (LLMResolver && LLMResolver->IsEnabled())
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ '%s' not resolved - attempting LLM fallback..."), *LocationName);
        
        FString Context = BuildSceneContext();
        FTransform LLMTransform = LLMResolver->ResolveLocation(LocationName, Context, nullptr);
        
        if (!LLMTransform.Equals(FTransform::Identity))
        {
            FVector LLMPosition = LLMTransform.GetLocation();
            UE_LOG(LogTemp, Display, TEXT("✅ LLM resolved '%s': [%.0f, %.0f, %.0f]"), 
                *LocationName, LLMPosition.X, LLMPosition.Y, LLMPosition.Z);
            return LLMPosition;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("❌ LLM fallback failed for '%s', trying 2.5D fallback"), *LocationName);
    }
    */
    // ========================================
    // 8. 2.5D SEMANTIC FALLBACK (Safety net)
    // ========================================
    UE_LOG(LogTemp, Warning, TEXT("   ⚠️ Using 2.5D fallback for '%s'"), *LocationName);
    
    if (UpperName.Contains(TEXT("CORNER")))
    {
        return GetRandomCornerPosition(UpperName);
    }
    
    if (UpperName.Contains(TEXT("BACKGROUND")) || 
        UpperName.Contains(TEXT("FAR")) || 
        UpperName.Contains(TEXT("DISTANT")))
    {
        return GetRandomBackgroundPosition();
    }
    
    if (UpperName.Contains(TEXT("FOREGROUND")) || 
        UpperName.Contains(TEXT("CLOSE")))
    {
        return GetRandomForegroundPosition();
    }
    
    if (UpperName.Contains(TEXT("OVERHEAD")) || 
        UpperName.Contains(TEXT("SKY")) || 
        UpperName.Contains(TEXT("CEILING")) ||
        UpperName.Contains(TEXT("ABOVE")))
    {
        return GetRandomOverheadPosition();
    }
    
    if (UpperName.Contains(TEXT("LEFT")))
    {
        FVector Anchor = GetBestAnchorFor("LEFT");
        if (!Anchor.IsZero()) return Anchor; // Found a smart point!
        
        // Fallback to dumb math if no anchors exist
        return GetRandomLeftSidePosition(); 
    }

    if (UpperName.Contains(TEXT("RIGHT")))
    {
        FVector Anchor = GetBestAnchorFor("RIGHT");
        if (!Anchor.IsZero()) return Anchor;
        
        return GetRandomRightSidePosition();
    }
    if (UpperName.Contains(TEXT("CENTER")) || UpperName.Contains(TEXT("MIDDLE")))
    {
        return GetRandomCenterPosition();
    }
    
    // ========================================
    // 9. SMART FALLBACK WITH UNIQUENESS
    // ========================================
    /*
     *REEMOVING THIS BECAUSE IT CAN FAIL LLM FALLBACK
    UE_LOG(LogTemp, Warning, TEXT("⚠️ '%s' - using smart positioning fallback"), *LocationName);
    
    TArray<FVector(ULocationQueryEngine::*)()> Strategies = {
        &ULocationQueryEngine::GetRandomCenterPosition,
        &ULocationQueryEngine::GetRandomLeftSidePosition,
        &ULocationQueryEngine::GetRandomRightSidePosition,
        &ULocationQueryEngine::GetRandomBackgroundPosition
    };
    
    for (int32 i = 0; i < Strategies.Num(); i++)
    {
        FVector Candidate = (this->*Strategies[i])();
        
        // Add random offset to prevent clustering
        Candidate += FVector(
            FMath::RandRange(-200.0f, 200.0f),
            FMath::RandRange(-200.0f, 200.0f),
            0.0f
        );
        
        if (IsLocationClear(Candidate, 150.0f))
        {
            UE_LOG(LogTemp, Display, TEXT("✅ Smart fallback success: (%.0f, %.0f, %.0f)"), 
                Candidate.X, Candidate.Y, Candidate.Z);
            return Candidate;
        }
    }
    */

    
    // ========================================
    // 10. EMERGENCY FALLBACK (Final resort)
    // ========================================
 /*
  *COMMENTING THIS OUT BECAUSE IT WILL PREVEN LLM FALLBACK 
  *   FVector Emergency = PlayableAreaCenter;
    Emergency.X += FMath::RandRange(-500.0f, 500.0f);
    Emergency.Y += FMath::RandRange(-500.0f, 500.0f);
    Emergency.Z = 300.0f; // Elevated so it's visible
    UE_LOG(LogTemp, Error, TEXT("❌ All fallbacks exhausted for '%s' - using emergency: (%.0f, %.0f, %.0f)"),
        *LocationName, Emergency.X, Emergency.Y, Emergency.Z);*/
    
    return FVector::Zero();
}


FSpawnLocation ULocationQueryEngine::ResolveLocation(const FString& LocationName)
{
    FString UpperName = LocationName.ToUpper();
    
    if (LocationDatabase.Contains(UpperName))
    {
        return LocationDatabase[UpperName];
    }
    
    FSpawnLocation DynamicLocation;
    DynamicLocation.LocationName = LocationName;
    DynamicLocation.WorldPosition = ResolveLocationName(LocationName);
    DynamicLocation.Description = TEXT("Dynamic location");

    // TODO: AI-Assisted Fallback - If WorldPosition is zero, query GenAI for semantic interpretation
    
    return DynamicLocation;
}

bool ULocationQueryEngine::DoesLocationExist(const FString& LocationName) const
{
    return LocationDatabase.Contains(LocationName.ToUpper());
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
// PLAYER-RELATIVE POSITIONS
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

FVector ULocationQueryEngine::GetPlayerPosition() const
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
        // 1. Update Database
        LocationDatabase[UpperName].bIsOccupied = bOccupied;
        LocationDatabase[UpperName].OccupyingActor = OccupyingActor;
        
        // Update the array copy
        for (FSpawnLocation& Loc : DiscoveredLocations)
        {
            if (Loc.LocationName.Equals(LocationName, ESearchCase::IgnoreCase))
            {
                Loc.bIsOccupied = bOccupied;
                Loc.OccupyingActor = OccupyingActor;
                break;
            }
        }

        // ✅ 2. UPDATE SPATIAL GRID
        if (bGridInitialized)
        {
            // CASE A: MARKING OCCUPIED
            if (bOccupied && IsValid(OccupyingActor))
            {
                // If this actor is already in the grid, remove it first (handle moving actors)
                if (OccupiedActorRegistry.Contains(OccupyingActor))
                {
                    FVector OldPos = OccupiedActorRegistry[OccupyingActor];
                    SpatialGrid.Get()->RemovePoint(OccupyingActor, (FVector3d)OldPos);
                }

                // Insert into Grid
                FVector NewPos = OccupyingActor->GetActorLocation();
                SpatialGrid.Get()->InsertPoint(OccupyingActor, (FVector3d)NewPos);
                OccupiedActorRegistry.Add(OccupyingActor, NewPos);
            }
            // CASE B: MARKING FREE (Removal)
            else if (!bOccupied && IsValid(OccupyingActor))
            {
                if (OccupiedActorRegistry.Contains(OccupyingActor))
                {
                    FVector OldPos = OccupiedActorRegistry[OccupyingActor];
                    SpatialGrid.Get()->RemovePoint(OccupyingActor, (FVector3d)OldPos);
                    OccupiedActorRegistry.Remove(OccupyingActor);
                }
            }
        }
    }
}

bool ULocationQueryEngine::IsLocationClear(FVector Location, float Radius) const
{
    if (!WorldContext) return false;

    // 1. LIFT CHECK ORIGIN (Your existing fix)
    float LiftAmount = Radius * 1.0f; 
    FVector CheckOrigin = Location + FVector(0, 0, LiftAmount);
    float CheckRadiusSq = FMath::Square(Radius); // Squared for fast math

    // =================================================================
    // ✅ CHECK 1: SPATIAL GRID (O(1) - SUPER FAST)
    // =================================================================
    if (bGridInitialized)
    {
        // We need to cast away const because FindAnyInRadius isn't const in UE5.0/5.1 in some versions
        auto& MutableGrid = const_cast<UE::Geometry::TPointHashGrid3<AActor*, double>&>(*SpatialGrid);

        bool bFoundBlocking = false;

        // FindAnyInRadius stops immediately upon finding ONE result
        MutableGrid.FindAnyInRadius(
            (FVector3d)Location, // Search center
            (double)Radius,      // Search radius
            
            // Distance Check Function
            [&](const AActor* Actor) { 
                if (!IsValid(Actor)) return 999999.0;
                return FVector::DistSquared(Location, Actor->GetActorLocation()); 
            },

            // Filter Function
            [&](const AActor* Actor) {
                // If we found a valid actor that isn't the player, it's a block!
                if (IsValid(Actor) && !Actor->ActorHasTag("Player.Character")) 
                {
                    bFoundBlocking = true;
                    return false; // Stop searching, we found a block
                }
                return true; // Continue searching
            }
        );

        if (bFoundBlocking) return false;
    }

    // =================================================================
    // CHECK 2: GEOMETRY (Walls/Floor - Keep this!)
    // =================================================================
    FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius * 0.8f); 
    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = false;
    QueryParams.AddIgnoredActor(GetPlayerPawn()); 

    FHitResult Hit;
    bool bHit = WorldContext->SweepSingleByChannel(
        Hit, CheckOrigin, CheckOrigin, FQuat::Identity,
        ECC_WorldStatic, SphereShape, QueryParams
    );
    
    if (bHit && Hit.bBlockingHit)
    {
        if (Hit.PenetrationDepth > 10.0f) return false;
    }

    return true;
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
    if (IsLocationClear(PreferredLocation, MinClearance))
    {
        FSpawnLocation Result;
        Result.LocationName = TEXT("Dynamic");
        Result.WorldPosition = PreferredLocation;
        Result.ClearanceRadius = MinClearance;
        return Result;
    }
    
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
    
    FSpawnLocation Result;
    Result.LocationName = TEXT("Dynamic");
    Result.WorldPosition = PreferredLocation;
    Result.ClearanceRadius = MinClearance;
    return Result;
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
// DYNAMIC ACTOR TAG
// ========================================

// LocationQueryEngine.cpp

TArray<AActor*> ULocationQueryEngine::GetActorsWithTag(const FString& Tag) const
{
    // 1. Check Cache
    // Note: Since this is a 'const' function, we must use const_cast to update the cache
    // or make the cache 'mutable' in the header.
    // For now, let's just cast away const for the cache update.
    auto* MutableThis = const_cast<ULocationQueryEngine*>(this);

    if (ActorTagCache.Contains(Tag))
    {
        return ActorTagCache[Tag].Actors; 
    }

    // 2. Fallback (Expensive Look up)
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(WorldContext, FName(*Tag), FoundActors);
    
    // ✅ MISSING LINE WAS HERE: Save to cache!
    MutableThis->ActorTagCache.Add(Tag).Actors = FoundActors; 

    return FoundActors;
}



TArray<FVector> ULocationQueryEngine::GetPositionsByTag(const FString& Tag) const
{
    TArray<FVector> Positions;
    
    TArray<AActor*> Actors = GetActorsWithTag(Tag);
    
    for (AActor* Actor : Actors)
    {
        if (IsValid(Actor))
        {
            Positions.Add(Actor->GetActorLocation());
        }
    }
    
    if (Positions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ GetPositionsByTag: No valid positions for tag '%s'"), *Tag);
    }
    
    return Positions;
}


AActor* ULocationQueryEngine::GetRandomActorWithTag(const FString& Tag)
{
    TArray<AActor*> Actors = GetActorsWithTag(Tag);
    
    if (Actors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ GetRandomActorWithTag: No actors with tag '%s'"), *Tag);
        return nullptr;
    }
    
    int32 RandomIndex = FMath::RandRange(0, Actors.Num() - 1);
    AActor* SelectedActor = Actors[RandomIndex];
    
    UE_LOG(LogTemp, Display, TEXT("🎲 Random actor for tag '%s': %s"), 
        *Tag, *SelectedActor->GetName());
    
    return SelectedActor;
}

FVector ULocationQueryEngine::GetRandomPositionFromTag(const FString& Tag)
{
    TArray<FVector> Positions = GetPositionsByTag(Tag);
    
    if (Positions.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ GetRandomPositionFromTag: No positions for tag '%s'"), *Tag);
        return FVector::ZeroVector;
    }
    
    int32 RandomIndex = FMath::RandRange(0, Positions.Num() - 1);
    FVector SelectedPos = Positions[RandomIndex];
    
    UE_LOG(LogTemp, Display, TEXT("🎲 Random position for tag '%s': [%.0f, %.0f, %.0f]"),
        *Tag, SelectedPos.X, SelectedPos.Y, SelectedPos.Z);
    
    return SelectedPos;
}

AActor* ULocationQueryEngine::GetClosestActorWithTag(const FString& Tag, FVector FromLocation)
{
    TArray<AActor*> Actors = GetActorsWithTag(Tag);
    
    if (Actors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ GetClosestActorWithTag: No actors with tag '%s'"), *Tag);
        return nullptr;
    }
    
    AActor* ClosestActor = Actors[0];
    float MinDistance = FVector::Dist(FromLocation, ClosestActor->GetActorLocation());
    
    for (AActor* Actor : Actors)
    {
        if (!IsValid(Actor)) continue;
        float Distance = FVector::Dist(FromLocation, Actor->GetActorLocation());
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            ClosestActor = Actor;
        }
    }
    
    UE_LOG(LogTemp, Display, TEXT("🔍 Closest '%s' actor: %.0f units away"), *Tag, MinDistance);
    return ClosestActor;
}

AActor* ULocationQueryEngine::GetFarthestActorWithTag(const FString& Tag, FVector FromLocation)
{
    TArray<AActor*> Actors = GetActorsWithTag(Tag);
    
    if (Actors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ GetFarthestActorWithTag: No actors with tag '%s'"), *Tag);
        return nullptr;
    }
    
    AActor* FarthestActor = Actors[0];
    float MaxDistance = FVector::Dist(FromLocation, FarthestActor->GetActorLocation());
    
    for (AActor* Actor : Actors)
    {
        if (!IsValid(Actor)) continue;
        float Distance = FVector::Dist(FromLocation, Actor->GetActorLocation());
        if (Distance > MaxDistance)
        {
            MaxDistance = Distance;
            FarthestActor = Actor;
        }
    }
    
    UE_LOG(LogTemp, Display, TEXT("🔍 Farthest '%s' actor: %.0f units away"), *Tag, MaxDistance);
    return FarthestActor;
}

TArray<FString> ULocationQueryEngine::GetAllActorTags() const
{
    TArray<FString> Tags;
    
    if (!WorldContext) return Tags;
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(WorldContext, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (!IsValid(Actor) || Actor->Tags.Num() == 0) continue;
        
        for (const FName& TagName : Actor->Tags)
        {
            FString TagStr = TagName.ToString();
            if (!TagStr.StartsWith(TEXT("Loc_")))
            {
                Tags.AddUnique(TagStr);
            }
        }
    }
    
    return Tags;
}


int32 ULocationQueryEngine::GetActorCountWithTag(const FString& Tag) const
{
    if (!WorldContext) return 0;
    
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(WorldContext, FName(*Tag.ToUpper()), FoundActors);
    
    return FoundActors.Num();
}

void ULocationQueryEngine::PrintActorTagCache() const
{
    if (!WorldContext) return;
    
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("╔═══════════════════════════════════════════╗"));
    UE_LOG(LogTemp, Warning, TEXT("║ 🏷️  Actor Tags in Scene ║"));
    UE_LOG(LogTemp, Warning, TEXT("╚═══════════════════════════════════════════╝"));
    
    TArray<FString> AllTags = GetAllActorTags();
    
    if (AllTags.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ No actor tags found"));
        return;
    }
    
    for (const FString& Tag : AllTags)
    {
        TArray<AActor*> TagActors = GetActorsWithTag(Tag);
        
        UE_LOG(LogTemp, Display, TEXT("🏷️  Tag: '%s'"), *Tag);
        UE_LOG(LogTemp, Display, TEXT("   Actors: %d"), TagActors.Num());
        
        for (int32 i = 0; i < TagActors.Num(); ++i)
        {
            AActor* Actor = TagActors[i];
            if (IsValid(Actor))
            {
                FVector Pos = Actor->GetActorLocation();
                UE_LOG(LogTemp, Display, TEXT("   [%d] %s at [%.0f, %.0f, %.0f]"),
                    i + 1, *Actor->GetName(), Pos.X, Pos.Y, Pos.Z);
            }
        }
        UE_LOG(LogTemp, Display, TEXT(""));
    }
}


void ULocationQueryEngine::VisualizeActorTagCache(float Duration, bool bCentered, float Radius)
{
    if (!WorldContext) return;
    
    TMap<FString, FColor> TagColors;
    TagColors.Add(TEXT("ENEMY"), FColor::Red);
    TagColors.Add(TEXT("ALLY"), FColor::Green);
    TagColors.Add(TEXT("ITEM"), FColor::Yellow);
    TagColors.Add(TEXT("NPC"), FColor::Cyan);
    TagColors.Add(TEXT("PLAYER"), FColor::Blue);
    TagColors.Add(TEXT("WALL"), FColor::Magenta);
    
    TArray<FString> AllTags = GetAllActorTags();
    
    for (const FString& Tag : AllTags)
    {
        FColor TagColor = TagColors.Contains(Tag.ToUpper()) 
            ? TagColors[Tag.ToUpper()] 
            : FColor::White;
        
        TArray<AActor*> TagActors = GetActorsWithTag(Tag);
        
        for (AActor* Actor : TagActors)
        {
            if (IsValid(Actor))
            {
                FVector ActorPos = Actor->GetActorLocation();
                DrawDebugSphere(WorldContext, ActorPos, Radius, 16, TagColor, false, Duration);
                DrawDebugString(WorldContext, ActorPos + FVector(0, 0, 100), *Actor->GetName(), 
                    nullptr, FColor::White, Duration);
            }
        }
    }
}

float ULocationQueryEngine::GetAverageActorDistance(const FString& Tag) const
{
    TArray<FVector> Positions = GetPositionsByTag(Tag);
    
    if (Positions.Num() <= 1) return 0.0f;
    
    float TotalDistance = 0.0f;
    int32 PairCount = 0;
    
    for (int32 i = 0; i < Positions.Num(); ++i)
    {
        for (int32 j = i + 1; j < Positions.Num(); ++j)
        {
            TotalDistance += FVector::Dist(Positions[i], Positions[j]);
            PairCount++;
        }
    }
    
    return PairCount > 0 ? TotalDistance / PairCount : 0.0f;
}
FVector ULocationQueryEngine::GetActorCentroid(const FString& Tag) const
{
    TArray<FVector> Positions = GetPositionsByTag(Tag);
    
    if (Positions.Num() == 0) return FVector::ZeroVector;
    
    FVector Sum = FVector::ZeroVector;
    for (const FVector& Pos : Positions)
    {
        Sum += Pos;
    }
    
    return Sum / Positions.Num();
}
FVector ULocationQueryEngine::FindSafeSpawnPosition(float MinClearance, int32 MaxAttempts)
{
    UE_LOG(LogTemp, Display, TEXT("🔍 Finding safe spawn position (clearance: %.0f)..."), MinClearance);

    // Strategy 1: Try existing free locations first
    TArray<FSpawnLocation> FreeLocations = GetFreeLocations();
    for (const FSpawnLocation& Loc : FreeLocations)
    {
        if (IsLocationClear(Loc.WorldPosition, MinClearance))
        {
            UE_LOG(LogTemp, Display, TEXT("✅ Found existing free location: %s"), *Loc.LocationName);
            return Loc.WorldPosition;
        }
    }

    // Strategy 2: Try random positions with validation
    TArray<FVector(ULocationQueryEngine::*)()> FallbackFunctions = {
        &ULocationQueryEngine::GetRandomCenterPosition,
        &ULocationQueryEngine::GetRandomBackgroundPosition,
        &ULocationQueryEngine::GetRandomLeftSidePosition,
        &ULocationQueryEngine::GetRandomRightSidePosition
    };

    for (int32 Attempt = 0; Attempt < MaxAttempts; Attempt++)
    {
        // Rotate through different fallback strategies
        int32 FuncIndex = Attempt % FallbackFunctions.Num();
        FVector Candidate = (this->*FallbackFunctions[FuncIndex])();
        if (!IsPositionInBounds(Candidate))
        {
            UE_LOG(LogTemp, Display, TEXT("⏭️ Attempt %d out of bounds, retrying..."), Attempt + 1);
            continue;
        }
        // Snap to ground
        Candidate = SnapToGround(Candidate, 1000.0f);

        // Validate clearance and spacing
        if (IsLocationClear(Candidate, MinClearance))
        {
            UE_LOG(LogTemp, Display, TEXT("✅ Found safe position after %d attempts: (%.0f, %.0f, %.0f)"), 
                Attempt + 1, Candidate.X, Candidate.Y, Candidate.Z);
            return Candidate;
        }

        UE_LOG(LogTemp, Display, TEXT("⏭️ Attempt %d/%d failed, retrying..."), Attempt + 1, MaxAttempts);
    }

    // Strategy 3: Emergency fallback - return position even if not perfect
    UE_LOG(LogTemp, Error, TEXT("❌ Could not find safe position after %d attempts!"), MaxAttempts);
    FVector Emergency = ClampPositionToBounds(PlayableAreaCenter);
    Emergency.Z += 200.0f;
    return Emergency;
}

void ULocationQueryEngine::PrintActorTagStats(const FString& Tag) const
{
    int32 Count = GetActorCountWithTag(Tag);
    float AvgDist = GetAverageActorDistance(Tag);
    FVector Centroid = GetActorCentroid(Tag);
    
    UE_LOG(LogTemp, Warning, TEXT("📊 Stats for tag '%s':"), *Tag);
    UE_LOG(LogTemp, Display, TEXT("   Actor Count: %d"), Count);
    UE_LOG(LogTemp, Display, TEXT("   Average Distance: %.2f units"), AvgDist);
    UE_LOG(LogTemp, Display, TEXT("   Centroid: [%.0f, %.0f, %.0f]"), Centroid.X, Centroid.Y, Centroid.Z);
}

// ========================================
// DEBUG & VISUALIZATION
// ========================================

void ULocationQueryEngine::PrintAllLocationData() const
{
    UE_LOG(LogTemp, Warning, TEXT(""));
    UE_LOG(LogTemp, Warning, TEXT("╔═══════════════════════════════════════════╗"));
    UE_LOG(LogTemp, Warning, TEXT("║   📊 Location Database Report             ║"));
    UE_LOG(LogTemp, Warning, TEXT("╚═══════════════════════════════════════════╝"));
    
    UE_LOG(LogTemp, Display, TEXT("Total Locations: %d"), DiscoveredLocations.Num());
    UE_LOG(LogTemp, Display, TEXT("Discovered Tags: %d"), DiscoveredLocationTags.Num());
    
    if (DiscoveredLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  No locations registered"));
        return;
    }
    
    for (int32 i = 0; i < DiscoveredLocations.Num(); ++i)
    {
        const FSpawnLocation& Loc = DiscoveredLocations[i];
        FString OccupiedStr = Loc.bIsOccupied ? TEXT("🔴 OCCUPIED") : TEXT("🟢 FREE");
        
        UE_LOG(LogTemp, Display, TEXT("[%d] %s | %s | Clearance: %.0f"),
            i + 1, *Loc.LocationName, *OccupiedStr, Loc.ClearanceRadius);
        UE_LOG(LogTemp, Display, TEXT("     Position: [%.0f, %.0f, %.0f]"),
            Loc.WorldPosition.X, Loc.WorldPosition.Y, Loc.WorldPosition.Z);
        UE_LOG(LogTemp, Display, TEXT("     Tags: %d | %s"),
            Loc.Tags.Num(), *FString::Join(Loc.Tags, TEXT(", ")));
        UE_LOG(LogTemp, Display, TEXT("     Desc: %s"), *Loc.Description);
    }
    
    UE_LOG(LogTemp, Display, TEXT(""));
}

void ULocationQueryEngine::VisualizeAllLocations(float Duration)
{
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  VisualizeAllLocations: No world context"));
        return;
    }
    
    UE_LOG(LogTemp, Display, TEXT("🎬 Visualizing %d locations for %.1f seconds"), 
        DiscoveredLocations.Num(), Duration);
    
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        FColor SphereColor = Loc.bIsOccupied ? FColor::Red : FColor::Green;
        DrawDebugSphere(WorldContext, Loc.WorldPosition, Loc.ClearanceRadius, 
                       16, SphereColor, false, Duration, 0, 1.0f);
        
        DrawDebugString(WorldContext, Loc.WorldPosition + FVector(0, 0, Loc.ClearanceRadius + 50.0f),
                       *Loc.LocationName, nullptr, FColor::White, Duration, false, 1.0f);
    }
}

void ULocationQueryEngine::PrintLocationsByStatus() const
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("📊 Location Status Report:"));
    
    int32 Occupied = 0, Free = 0;
    
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        if (Loc.bIsOccupied)
        {
            Occupied++;
            UE_LOG(LogTemp, Display, TEXT("   🔴 %s - OCCUPIED"), *Loc.LocationName);
        }
        else
        {
            Free++;
        }
    }
    
    UE_LOG(LogTemp, Display, TEXT("   🟢 %d free locations"), Free);
    UE_LOG(LogTemp, Display, TEXT("   🔴 %d occupied locations"), Occupied);
    UE_LOG(LogTemp, Display, TEXT(""));
}



// ========================================
// ADVANCED QUERIES
// ========================================

FSpawnLocation ULocationQueryEngine::FindClosestLocation(FVector FromPosition) const
{
    if (DiscoveredLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  FindClosestLocation: No locations available"));
        return FSpawnLocation();
    }
    
    FSpawnLocation ClosestLoc = DiscoveredLocations[0];
    float MinDistance = FVector::Dist(FromPosition, ClosestLoc.WorldPosition);
    
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        float Distance = FVector::Dist(FromPosition, Loc.WorldPosition);
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            ClosestLoc = Loc;
        }
    }
    
    UE_LOG(LogTemp, Display, TEXT("🔍 Closest to [%.0f, %.0f, %.0f]: %s (%.0f units)"),
        FromPosition.X, FromPosition.Y, FromPosition.Z, *ClosestLoc.LocationName, MinDistance);
    
    return ClosestLoc;
}

FSpawnLocation ULocationQueryEngine::FindFarthestLocation(FVector FromPosition) const
{
    if (DiscoveredLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  FindFarthestLocation: No locations available"));
        return FSpawnLocation();
    }
    
    FSpawnLocation FarthestLoc = DiscoveredLocations[0];
    float MaxDistance = FVector::Dist(FromPosition, FarthestLoc.WorldPosition);
    
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        float Distance = FVector::Dist(FromPosition, Loc.WorldPosition);
        if (Distance > MaxDistance)
        {
            MaxDistance = Distance;
            FarthestLoc = Loc;
        }
    }
    
    UE_LOG(LogTemp, Display, TEXT("🔍 Farthest from [%.0f, %.0f, %.0f]: %s (%.0f units)"),
        FromPosition.X, FromPosition.Y, FromPosition.Z, *FarthestLoc.LocationName, MaxDistance);
    
    return FarthestLoc;
}

TArray<FSpawnLocation> ULocationQueryEngine::FindLocationsInRadius(FVector Center, float Radius) const
{
    TArray<FSpawnLocation> InRadius;
    
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        float Distance = FVector::Dist(Center, Loc.WorldPosition);
        if (Distance <= Radius)
        {
            InRadius.Add(Loc);
        }
    }
    
    UE_LOG(LogTemp, Display, TEXT("🔍 Found %d locations within %.0f radius"), 
        InRadius.Num(), Radius);
    
    return InRadius;
}

int32 ULocationQueryEngine::GetFreeLocationCount() const
{
    int32 Count = 0;
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        if (!Loc.bIsOccupied)
        {
            Count++;
        }
    }
    return Count;
}

int32 ULocationQueryEngine::GetOccupiedLocationCount() const
{
    return DiscoveredLocations.Num() - GetFreeLocationCount();
}

// ========================================
// BATCH LOCATION OPERATIONS
// ========================================

void ULocationQueryEngine::AddMultipleLocations(const TArray<FSpawnLocation>& NewLocations)
{
    if (NewLocations.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ AddMultipleLocations: No locations to add"));
        return;
    }
    
    int32 AddedCount = 0;
    
    for (const FSpawnLocation& Location : NewLocations)
    {
        if (Location.LocationName.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("⚠️ Skipping location with empty name"));
            continue;
        }
        
        FString UpperName = Location.LocationName.ToUpper();
        
        // Add to database
        LocationDatabase.Add(UpperName, Location);
        DiscoveredLocations.Add(Location);
        
        // Add tags
        for (const FString& Tag : Location.Tags)
        {
            DiscoveredLocationTags.AddUnique(Tag);
        }
        
        AddedCount++;
    }
    
    UE_LOG(LogTemp, Display, TEXT("✅ AddMultipleLocations: Added %d locations (attempted %d)"), 
        AddedCount, NewLocations.Num());
}

bool ULocationQueryEngine::RemoveMultipleLocations(const TArray<FString>& LocationNames)
{
    if (LocationNames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ RemoveMultipleLocations: No locations to remove"));
        return false;
    }
    
    int32 RemovedCount = 0;
    
    for (const FString& LocationName : LocationNames)
    {
        if (RemoveLocation(LocationName))
        {
            RemovedCount++;
        }
    }
    
    bool bAllRemoved = (RemovedCount == LocationNames.Num());
    
    UE_LOG(LogTemp, Display, TEXT("✅ RemoveMultipleLocations: Removed %d locations (attempted %d)"), 
        RemovedCount, LocationNames.Num());
    
    return bAllRemoved;
}

// ========================================
// SPAWN VALIDATION & CLEARANCE
// ========================================

bool ULocationQueryEngine::IsLocationValidForSpawn(const FString& LocationName, float MinClearance) const
{
    FString UpperName = LocationName.ToUpper();
    
    // 1. Check if location exists
    if (!LocationDatabase.Contains(UpperName))
    {
        UE_LOG(LogTemp, Display, TEXT("❌ IsLocationValidForSpawn: Location '%s' not found"), *LocationName);
        return false;
    }
    
    const FSpawnLocation& Location = LocationDatabase[UpperName];
    
    // 2. Check if location is occupied
    if (Location.bIsOccupied)
    {
        UE_LOG(LogTemp, Display, TEXT("❌ IsLocationValidForSpawn: Location '%s' is occupied"), *LocationName);
        return false;
    }
    
    // 3. Check clearance if provided
    if (MinClearance > 0.0f && Location.ClearanceRadius < MinClearance)
    {
        UE_LOG(LogTemp, Display, TEXT("❌ IsLocationValidForSpawn: Location '%s' clearance %.0f < required %.0f"), 
            *LocationName, Location.ClearanceRadius, MinClearance);
        return false;
    }
    
    // 4. Check if area is actually clear (collision test)
    if (!IsLocationClear(Location.WorldPosition, MinClearance > 0.0f ? MinClearance : Location.ClearanceRadius))
    {
        UE_LOG(LogTemp, Display, TEXT("❌ IsLocationValidForSpawn: Location '%s' has blocking collision"), *LocationName);
        return false;
    }
    
    UE_LOG(LogTemp, Display, TEXT("✅ IsLocationValidForSpawn: Location '%s' is valid"), *LocationName);
    return true;
}

FSpawnLocation ULocationQueryEngine::FindValidSpawnLocation(const FString& PreferredLocation, float MinClearance)
{
    FString UpperName = PreferredLocation.ToUpper();
    UE_LOG(LogTemp, Display, TEXT("🔎 FindValidSpawnLocation: Preferred='%s', MinClearance=%.0f"), 
        *PreferredLocation, MinClearance);

    // 1. Try preferred location first
    if (LocationDatabase.Contains(UpperName))
    {
        if (IsLocationValidForSpawn(PreferredLocation, MinClearance))
        {
            UE_LOG(LogTemp, Display, TEXT("✅ Found preferred location: %s"), *PreferredLocation);
            return LocationDatabase[UpperName];
        }
        UE_LOG(LogTemp, Display, TEXT("⚠️ Preferred location invalid, searching alternatives..."));
    }

    // 2. Find ANY free location with sufficient clearance
    for (const FSpawnLocation& Location : DiscoveredLocations)
    {
        if (IsLocationValidForSpawn(Location.LocationName, MinClearance))
        {
            UE_LOG(LogTemp, Display, TEXT("✅ Found alternative: %s"), *Location.LocationName);
            return Location;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("⚠️ No valid locations found, using smart fallback..."));

    // 3. Use smart fallback with validation (NEW)
    FVector SafePos = FindSafeSpawnPosition(MinClearance, 60);
    
    FSpawnLocation DynamicLocation;
    DynamicLocation.LocationName = FString::Printf(TEXT("DYNAMIC_SPAWN_%s"), *FGuid::NewGuid().ToString().Left(8));
    DynamicLocation.WorldPosition = SafePos;
    DynamicLocation.ClearanceRadius = MinClearance;
    DynamicLocation.Description = TEXT("Dynamically validated spawn location");
    DynamicLocation.Tags.Add(TEXT("Dynamic"));
    
    // Add to database for tracking
    AddLocation(DynamicLocation);
    
    UE_LOG(LogTemp, Display, TEXT("✅ Created dynamic fallback: %s at (%.0f, %.0f, %.0f)"), 
        *DynamicLocation.LocationName, SafePos.X, SafePos.Y, SafePos.Z);
    
    return DynamicLocation;
}

// ========================================
// 2.5D FIGHTING GAME FALLBACK POSITIONS
// ========================================

FVector ULocationQueryEngine::GetRandomCornerPosition(const FString& CornerType)
{
    if (!bBoundsInitialized)
    {
        return FVector::ZeroVector;
    }
    
    // Determine corner based on bounds
    bool bIsLeft = CornerType.Contains(TEXT("LEFT"));
    bool bIsBack = CornerType.Contains(TEXT("BACK"));
    
    // Base position at corner
    float XBase = bIsLeft ? PlayableAreaBounds.Min.X : PlayableAreaBounds.Max.X;
    float YBase = bIsBack ? PlayableAreaBounds.Max.Y : PlayableAreaBounds.Min.Y;
    
    // Add small randomization (10% of bounds size)
    float XRange = (PlayableAreaBounds.Max.X - PlayableAreaBounds.Min.X) * 0.05f;
    float YRange = (PlayableAreaBounds.Max.Y - PlayableAreaBounds.Min.Y) * 0.05f;
    
    float X = XBase + FMath::RandRange(-XRange, XRange);
    float Y = YBase + FMath::RandRange(-YRange, YRange);
    float Z = FMath::RandRange(GroundHeightRange.X, GroundHeightRange.Y);
    
    FVector Pos = FVector(X, Y, Z);
    UE_LOG(LogTemp, Display, TEXT("Corner fallback (%s): (%.0f, %.0f, %.0f)"), *CornerType, Pos.X, Pos.Y, Pos.Z);
    return Pos;
}


FVector ULocationQueryEngine::GetRandomBackgroundPosition()
{
    if (!bBoundsInitialized)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ Bounds not initialized! Call InitializePlayableAreaBounds() first"));
        return FVector::ZeroVector;
    }
    
    // Background = far from camera (high Y values in 2.5D)
    float YMin = FMath::Lerp(PlayableAreaBounds.Min.Y, PlayableAreaBounds.Max.Y, 0.6f); // 60% back
    float YMax = PlayableAreaBounds.Max.Y;
    
    float X = FMath::RandRange(PlayableAreaBounds.Min.X, PlayableAreaBounds.Max.X);
    float Y = FMath::RandRange(YMin, YMax);
    float Z = FMath::RandRange(GroundHeightRange.X, GroundHeightRange.Y);
    
    FVector Pos = FVector(X, Y, Z);
    UE_LOG(LogTemp, Display, TEXT("Background fallback: (%.0f, %.0f, %.0f)"), Pos.X, Pos.Y, Pos.Z);
    return Pos;
}


FVector ULocationQueryEngine::GetRandomForegroundPosition()
{
    if (!bBoundsInitialized)
    {
        return FVector::ZeroVector;
    }
    
    // Foreground = close to camera (low Y values in 2.5D)
    float YMin = PlayableAreaBounds.Min.Y;
    float YMax = FMath::Lerp(PlayableAreaBounds.Min.Y, PlayableAreaBounds.Max.Y, 0.3f); // Front 30%
    
    float X = FMath::RandRange(PlayableAreaBounds.Min.X, PlayableAreaBounds.Max.X);
    float Y = FMath::RandRange(YMin, YMax);
    float Z = FMath::RandRange(GroundHeightRange.X, GroundHeightRange.Y);
    
    FVector Pos = FVector(X, Y, Z);
    UE_LOG(LogTemp, Display, TEXT("Foreground fallback: (%.0f, %.0f, %.0f)"), Pos.X, Pos.Y, Pos.Z);
    return Pos;
}


FVector ULocationQueryEngine::GetRandomOverheadPosition()
{
    if (!bBoundsInitialized)
    {
        return FVector::ZeroVector;
    }
    
    // Overhead = aerial height (particles, ceiling objects)
    float X = FMath::RandRange(PlayableAreaBounds.Min.X, PlayableAreaBounds.Max.X);
    float Y = FMath::RandRange(PlayableAreaBounds.Min.Y, PlayableAreaBounds.Max.Y);
    float Z = FMath::RandRange(AerialHeightRange.X, AerialHeightRange.Y);
    
    FVector Pos = FVector(X, Y, Z);
    UE_LOG(LogTemp, Display, TEXT("Overhead fallback: (%.0f, %.0f, %.0f)"), Pos.X, Pos.Y, Pos.Z);
    return Pos;
}


FVector ULocationQueryEngine::GetRandomLeftSidePosition()
{
    if (!bBoundsInitialized)
    {
        return FVector::ZeroVector;
    }
    
    // Left side = left 40% of arena (negative X in UE5)
    float XMin = PlayableAreaBounds.Min.X;
    float XMax = FMath::Lerp(PlayableAreaBounds.Min.X, PlayableAreaBounds.Max.X, 0.4f);
    
    float X = FMath::RandRange(XMin, XMax);
    float Y = FMath::RandRange(PlayableAreaBounds.Min.Y, PlayableAreaBounds.Max.Y);
    float Z = FMath::RandRange(GroundHeightRange.X, GroundHeightRange.Y);
    
    FVector Pos = FVector(X, Y, Z);
    UE_LOG(LogTemp, Display, TEXT("Left side fallback: (%.0f, %.0f, %.0f)"), Pos.X, Pos.Y, Pos.Z);
    return Pos;
}

FVector ULocationQueryEngine::GetRandomRightSidePosition()
{
    if (!bBoundsInitialized)
    {
        return FVector::ZeroVector;
    }
    
    // Right side = right 40% of arena (positive X in UE5)
    float XMin = FMath::Lerp(PlayableAreaBounds.Min.X, PlayableAreaBounds.Max.X, 0.6f);
    float XMax = PlayableAreaBounds.Max.X;
    
    float X = FMath::RandRange(XMin, XMax);
    float Y = FMath::RandRange(PlayableAreaBounds.Min.Y, PlayableAreaBounds.Max.Y);
    float Z = FMath::RandRange(GroundHeightRange.X, GroundHeightRange.Y);
    
    FVector Pos = FVector(X, Y, Z);
    UE_LOG(LogTemp, Display, TEXT("Right side fallback: (%.0f, %.0f, %.0f)"), Pos.X, Pos.Y, Pos.Z);
    return Pos;
}


FVector ULocationQueryEngine::GetRandomCenterPosition()
{
    if (!bBoundsInitialized)
    {
        return FVector::ZeroVector;
    }
    
    // Center = middle 40% of arena
    FVector Center = PlayableAreaCenter;
    float XRange = (PlayableAreaBounds.Max.X - PlayableAreaBounds.Min.X) * 0.2f; // ±20%
    float YRange = (PlayableAreaBounds.Max.Y - PlayableAreaBounds.Min.Y) * 0.2f;
    
    float X = FMath::RandRange(Center.X - XRange, Center.X + XRange);
    float Y = FMath::RandRange(Center.Y - YRange, Center.Y + YRange);
    float Z = FMath::RandRange(GroundHeightRange.X, GroundHeightRange.Y);
    
    return FVector(X, Y, Z);
}

FVector ULocationQueryEngine::GetLocationCentroid() const
{
    if (DiscoveredLocations.Num() == 0) return FVector::ZeroVector;
    
    FVector Sum = FVector::ZeroVector;
    for (const FSpawnLocation& Loc : DiscoveredLocations)
    {
        Sum += Loc.WorldPosition;
    }
    
    return Sum / DiscoveredLocations.Num();
}

float ULocationQueryEngine::GetAverageLocationDistance() const
{
    if (DiscoveredLocations.Num() < 2) return 0.0f;
    
    float TotalDistance = 0.0f;
    int32 ComparisionCount = 0;
    
    for (int32 i = 0; i < DiscoveredLocations.Num(); ++i)
    {
        for (int32 j = i + 1; j < DiscoveredLocations.Num(); ++j)
        {
            TotalDistance += FVector::Dist(
                DiscoveredLocations[i].WorldPosition,
                DiscoveredLocations[j].WorldPosition
            );
            ComparisionCount++;
        }
    }
    
    return ComparisionCount > 0 ? TotalDistance / ComparisionCount : 0.0f;
}

void ULocationQueryEngine::PrintLocationStats() const
{
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("📈 Location Statistics:"));
    UE_LOG(LogTemp, Display, TEXT("   Total: %d"), DiscoveredLocations.Num());
    UE_LOG(LogTemp, Display, TEXT("   Free: %d"), GetFreeLocationCount());
    UE_LOG(LogTemp, Display, TEXT("   Occupied: %d"), GetOccupiedLocationCount());
    UE_LOG(LogTemp, Display, TEXT("   Avg Distance: %.0f units"), GetAverageLocationDistance());
    UE_LOG(LogTemp, Display, TEXT("   Centroid: %s"), *GetLocationCentroid().ToString());
    UE_LOG(LogTemp, Display, TEXT(""));
}

void ULocationQueryEngine::RenameLocation(const FString& OldName, const FString& NewName)
{
    FString UpperOld = OldName.ToUpper();
    FString UpperNew = NewName.ToUpper();
    
    if (!LocationDatabase.Contains(UpperOld))
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️  RenameLocation: '%s' not found"), *OldName);
        return;
    }
    
    FSpawnLocation Loc = LocationDatabase[UpperOld];
    Loc.LocationName = NewName;
    
    RemoveLocation(OldName);
    AddLocation(Loc);
    
    UE_LOG(LogTemp, Display, TEXT("✅ Renamed '%s' → '%s'"), *OldName, *NewName);
}

FString ULocationQueryEngine::GetLocationContextForLLM() const
{
    FString Context = TEXT("=== AVAILABLE SPAWN LOCATIONS ===\n\n");
    
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
    
    Context += TEXT("[Player-Relative]\n");
    Context += TEXT("  - \"PLAYER_FRONT\", \"PLAYER_BACK\", \"PLAYER_LEFT\", \"PLAYER_RIGHT\"\n");
    Context += TEXT("  - \"PLAYER_POSITION\", \"NEAR_PLAYER\"\n\n");
    
    Context += TEXT("[Actor Tags]\n");
    Context += TEXT("  - \"NEAR:TagName\" for random actor with tag\n");
    Context += TEXT("  - \"CLOSEST:TagName\" for nearest actor to player\n\n");
    
    Context += TEXT("[Custom Coordinates]\n");
    Context += TEXT("  - Format: \"CUSTOM:[X,Y,Z]\"\n\n");
    
    return Context;
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
    
    for (AActor* Actor : FoundActors)
    {
        if (IsValid(Actor))
        {
            return Actor;
        }
    }
    
    return nullptr;
}

void ULocationQueryEngine::BeginDestroy()
{
    UE_LOG(LogTemp, Display, TEXT("🗑️  LocationQueryEngine destroyed"));
    Super::BeginDestroy();
}

// ========================================
// TAG-BASED ACTOR QUERIES (Functions only)
// ========================================

AActor* ULocationQueryEngine::FindRandomActorWithTag(const FString& Tag)
{
    if (!WorldContext || Tag.IsEmpty())
    {
        return nullptr;
    }
    
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(WorldContext, FName(*Tag.ToUpper()), FoundActors);
    
    if (FoundActors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ FindRandomActorWithTag: No actors with tag '%s'"), *Tag);
        return nullptr;
    }
    
    int32 RandomIndex = FMath::RandRange(0, FoundActors.Num() - 1);
    AActor* SelectedActor = FoundActors[RandomIndex];
    
    UE_LOG(LogTemp, Display, TEXT("🎲 Found random '%s' actor: %s"), 
        *Tag, IsValid(SelectedActor) ? *SelectedActor->GetName() : TEXT("invalid"));
    
    return SelectedActor;
}

AActor* ULocationQueryEngine::FindClosestActorWithTag(const FString& Tag, FVector FromLocation)
{
    if (!WorldContext || Tag.IsEmpty())
    {
        return nullptr;
    }
    
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithTag(WorldContext, FName(*Tag.ToUpper()), FoundActors);
    
    if (FoundActors.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠️ FindClosestActorWithTag: No actors with tag '%s'"), *Tag);
        return nullptr;
    }
    
    AActor* ClosestActor = FoundActors[0];
    float MinDistance = FVector::Dist(FromLocation, ClosestActor->GetActorLocation());
    
    for (AActor* Actor : FoundActors)
    {
        if (!IsValid(Actor)) continue;
        
        float Distance = FVector::Dist(FromLocation, Actor->GetActorLocation());
        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            ClosestActor = Actor;
        }
    }
    
    UE_LOG(LogTemp, Display, TEXT("🔍 Found closest '%s' actor: %.0f units away"), *Tag, MinDistance);
    
    return ClosestActor;
}

// ========================================
// LLM FALLBACK INTEGRATION
// ========================================

void ULocationQueryEngine::ConfigureLLMFallback(
    const FString& Endpoint,
    const FString& APIKey,
    const FString& ModelName)
{
    if (!LLMResolver)
    {
        LLMResolver = NewObject<ULocationResolverLLM>(this);
        UE_LOG(LogTemp, Display, TEXT("✅ LocationEngine: Created LLM resolver"));
    }
    
    LLMResolver->Configure(Endpoint, APIKey, ModelName);
    
    UE_LOG(LogTemp, Display, TEXT("✅ LocationEngine: LLM fallback %s"), 
        LLMResolver->IsEnabled() ? TEXT("ENABLED") : TEXT("DISABLED"));
}

FString ULocationQueryEngine::BuildSceneContext() const
{
    FVector PlayerPos = GetPlayerPosition();
   
    return FString::Printf(
        TEXT("ARENA BOUNDS:\n"
             "X: %d to %d (center: %d)\n"
             "Y: %d to %d (center: %d)\n"
             "Z: %d to %d (ground: %d)\n"
             "\n"
             "PLAYER POSITION: [%d, %d, %d]\n"
             "\n"
             "GROUND HEIGHT: %.0f to %.0f\n"
             "AERIAL HEIGHT: %.0f to %.0f"

             ),
        (int32)PlayableAreaBounds.Min.X, (int32)PlayableAreaBounds.Max.X, (int32)PlayableAreaCenter.X,
        (int32)PlayableAreaBounds.Min.Y, (int32)PlayableAreaBounds.Max.Y, (int32)PlayableAreaCenter.Y,
        (int32)PlayableAreaBounds.Min.Z, (int32)PlayableAreaBounds.Max.Z, (int32)GroundHeightRange.X,
        (int32)PlayerPos.X, (int32)PlayerPos.Y, (int32)PlayerPos.Z,
        GroundHeightRange.X, GroundHeightRange.Y,
        AerialHeightRange.X, AerialHeightRange.Y
    );
}


FVector ULocationQueryEngine::GetBestAnchorFor(const FString& Tag, float MinClearance)
{
    // 1. Get all pre-scanned locations that match this tag
    // (You already have GetLocationsByTag implemented!)
    TArray<FSpawnLocation> Candidates = GetLocationsByTag(Tag);

    FSpawnLocation BestAnchor;
    float BestScore = -1.0f;
    bool bFoundValid = false;

    FVector PlayerPos = GetPlayerPosition();

    for (const FSpawnLocation& Loc : Candidates)
    {
        // --- CRITERIA 1: MUST BE FREE ---
        if (Loc.bIsOccupied) continue;
        
        // --- CRITERIA 2: MUST BE CLEAR (Physics) ---
        // Uses your Spatial Grid + Sweep check
        if (!IsLocationClear(Loc.WorldPosition, MinClearance)) continue;

        // --- CRITERIA 3: SCORING (Simple Heuristic) ---
        float Score = 100.0f;
        
        // Penalty: Too close to player (don't spawn on top of user)
        float Dist = FVector::Dist(Loc.WorldPosition, PlayerPos);
        if (Dist < 300.0f) Score -= 50.0f;
        
        // Bonus: "Anchor" tag implies it was manually placed for a good reason
        if (Loc.Tags.Contains("Anchor")) Score += 10.0f;

        // Keep the best one
        if (Score > BestScore)
        {
            BestScore = Score;
            BestAnchor = Loc;
            bFoundValid = true;
        }
    }

    if (bFoundValid)
    {
        UE_LOG(LogTemp, Display, TEXT("✅ Anchor Found: '%s' (Tag: %s)"), *BestAnchor.LocationName, *Tag);
        return BestAnchor.WorldPosition;
    }

    // Return ZeroVector to signal "Use Procedural Fallback"
    return FVector::ZeroVector;
}
