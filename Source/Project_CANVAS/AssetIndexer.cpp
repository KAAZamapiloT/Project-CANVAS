// Fill out your copyright notice in the Description page of Project Settings.


#include "AssetIndexer.h"
#include "AssetRegistry/AssetRegistryModule.h"
void UAssetIndexer::ScanForTextures(FString ScanPath)
{
	// Clear any old results
	DiscoveredTextureNames.Empty();

	// 1. Load the Asset Registry module
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// 2. Create a filter to find *only* the assets we want
	FARFilter Filter;
	Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*ScanPath)); // FName constructor needs *FString
	Filter.bRecursivePaths = true; // Search all subfolders in this path

	// 3. Run the query
	TArray<FAssetData> AssetDataArray;
	AssetRegistryModule.Get().GetAssets(Filter, AssetDataArray);

	UE_LOG(LogTemp, Warning, TEXT("AssetIndexer found %d textures in %s"), AssetDataArray.Num(), *ScanPath);

	// 4. Store the names in our "database"
	for (const FAssetData& AssetData : AssetDataArray)
	{
		// We just want the simple name, e.g., "T_Brick_Normal"
		FString AssetName = AssetData.AssetName.ToString();
		DiscoveredTextureNames.Add(AssetName);
	}
}

TArray<FString> UAssetIndexer::GetDiscoveredTexturesName() const
{
	return DiscoveredTextureNames;
}
