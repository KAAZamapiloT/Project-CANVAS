// Fill out your copyright notice in the Description page of Project Settings.


// AssetIndexer.cpp
#include "AssetIndexer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/Async.h"
#include "Engine/Texture2D.h"
#include "Particles/ParticleSystem.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"

void UAssetIndexer::ScanAllAssetsAsync(UWorld* WorldContext)
{
    if (bIsScanning)
    {
        UE_LOG(LogTemp, Warning, TEXT("AssetIndexer: Scan already in progress."));
        return;
    }

    bIsScanning = true;
    bIsScanComplete = false;
    PendingScans = 0;
    
    // Clear old data
    DiscoveredTextureNames.Empty();
    DiscoveredParticleNames.Empty();
    DiscoveredPostProcessNames.Empty();
    DiscoveredActorTags.Empty();

    UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Starting comprehensive asset scan..."));

    // Launch all scans
    PendingScans = 4; // Textures, Particles, PostProcess, ActorTags
    
    ScanForTexturesAsync(TEXT("/Game/DATABASE/textures"));
    ScanForParticlesAsync(TEXT("/Game/DATABASE/particles"));
    ScanForPostProcessMaterialsAsync(TEXT("/Game/DATABASE/postprocess"));
    ScanActorTagsInLevel(WorldContext);
}

void UAssetIndexer::ScanForTexturesAsync(FString ScanPath)
{
    AsyncTask(ENamedThreads::GameThread, [this, ScanPath]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning textures in %s"), *ScanPath);
        ScanAssetsOfType(UTexture2D::StaticClass(), ScanPath, DiscoveredTextureNames);
        CheckAllScansComplete();
    });
}

void UAssetIndexer::ScanForParticlesAsync(FString ScanPath)
{
    AsyncTask(ENamedThreads::GameThread, [this, ScanPath]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning particles in %s"), *ScanPath);
        ScanAssetsOfType(UParticleSystem::StaticClass(), ScanPath, DiscoveredParticleNames);
        CheckAllScansComplete();
    });
}

void UAssetIndexer::ScanForPostProcessMaterialsAsync(FString ScanPath)
{
    AsyncTask(ENamedThreads::GameThread, [this, ScanPath]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning post-process materials in %s"), *ScanPath);
        ScanAssetsOfType(UMaterialInterface::StaticClass(), ScanPath, DiscoveredPostProcessNames);
        CheckAllScansComplete();
    });
}

void UAssetIndexer::ScanActorTagsInLevel(UWorld* WorldContext)
{
    if (!WorldContext)
    {
        UE_LOG(LogTemp, Error, TEXT("AssetIndexer: WorldContext is null!"));
        CheckAllScansComplete();
        return;
    }

    AsyncTask(ENamedThreads::GameThread, [this, WorldContext]()
    {
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Scanning actor tags in current level..."));
        
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(WorldContext, AActor::StaticClass(), AllActors);
        
        TSet<FString> UniqueTagsSet; // Use set to avoid duplicates
        
        for (AActor* Actor : AllActors)
        {
            if (Actor)
            {
                for (const FName& Tag : Actor->Tags)
                {
                    // Only add tags that follow your naming convention
                    FString TagString = Tag.ToString();
                    if (TagString.Contains(".")) // e.g., "Background.Wall"
                    {
                        UniqueTagsSet.Add(TagString);
                    }
                }
            }
        }
        
        // Convert set to array
        DiscoveredActorTags = UniqueTagsSet.Array();
        
        UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d unique actor tags"), DiscoveredActorTags.Num());
        CheckAllScansComplete();
    });
}

void UAssetIndexer::ScanAssetsOfType(const UClass* AssetClass, FString ScanPath, TArray<FString>& OutArray)
{
    if (!AssetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("AssetIndexer: AssetClass is null!"));
        return;
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    
    FARFilter Filter;
    Filter.ClassPaths.Add(AssetClass->GetClassPathName());
    Filter.PackagePaths.Add(FName(*ScanPath));
    Filter.bRecursivePaths = true;

    TArray<FAssetData> AssetDataArray;
    AssetRegistryModule.Get().GetAssets(Filter, AssetDataArray);

    OutArray.Empty();
    for (const FAssetData& AssetData : AssetDataArray)
    {
        OutArray.Add(AssetData.AssetName.ToString());
    }

    UE_LOG(LogTemp, Log, TEXT("AssetIndexer: Found %d assets of type %s"), 
           OutArray.Num(), *AssetClass->GetName());
}

void UAssetIndexer::CheckAllScansComplete()
{
    PendingScans--;
    
    if (PendingScans <= 0)
    {
        bIsScanning = false;
        bIsScanComplete = true;
        
        UE_LOG(LogTemp, Warning, TEXT("AssetIndexer: ========== SCAN COMPLETE =========="));
        UE_LOG(LogTemp, Warning, TEXT("  Textures: %d"), DiscoveredTextureNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  Particles: %d"), DiscoveredParticleNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  PostProcess: %d"), DiscoveredPostProcessNames.Num());
        UE_LOG(LogTemp, Warning, TEXT("  Actor Tags: %d"), DiscoveredActorTags.Num());
        UE_LOG(LogTemp, Warning, TEXT("================================================"));
        
        OnScanComplete.Broadcast();
    }
}
