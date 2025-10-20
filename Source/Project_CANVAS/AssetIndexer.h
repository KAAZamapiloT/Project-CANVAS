// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AssetIndexer.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_CANVAS_API UAssetIndexer : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
	void ScanForTextures(FString ScanPath);
	UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
	TArray<FString>  GetDiscoveredTexturesName() const;
private:
	UPROPERTY()
	TArray<FString> DiscoveredTextureNames;
	/**
	 *MIGHT BE FUTURE WORK
	 

	UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
	FString GetDiscoveredMaterialsName();

	UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
	FString GetDiscoveredModelsName();

	UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
	FString GetDiscoveredSoundsName();

	UFUNCTION(BlueprintCallable, Category = "AssetIndexer")
	void GetDiscoveredMeshesName();

	**/
};
