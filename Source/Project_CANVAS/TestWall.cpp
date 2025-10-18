// Fill out your copyright notice in the Description page of Project Settings.


#include "TestWall.h"
#include "JsonParser.h" // Your parser class
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

// Sets default values
ATestWall::ATestWall()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	Tags.Add("Background.Wall");
}

// Called when the game starts or when spawned
void ATestWall::BeginPlay()
{
	Super::BeginPlay();

	Parser = NewObject<UJsonParser>();
	MySceneBuilder = NewObject<USceneBuilder>();
    FString FilePath = FPaths::ProjectContentDir() + TEXT("TestWall.json");
	FString JsonString;
	if (FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		// 3. SUCCESS: The file was found and read into JsonString
		UE_LOG(LogTemp, Log, TEXT("File loaded successfully from: %s"), *FilePath);

		// 4. Now, call the parser with the valid string
		FEnhancedScenePlan MyPlan = Parser->CreatePlan(JsonString);

		// 5. Call the SceneBuilder
		if (MySceneBuilder)
		{
			MySceneBuilder->BuildScene(MyPlan, GetWorld());
		}
	}
	else
	{
		// 3. FAILURE: The file was NOT found
		UE_LOG(LogTemp, Error, TEXT("FATAL ERROR: Failed to load file at path: %s"), *FilePath);
		UE_LOG(LogTemp, Error, TEXT("Make sure 'TestWall.json' is in your project's 'Content' folder!"));
	}
	
}

// Called every frame
void ATestWall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// just  a test function
void ATestWall::ChangeColor(FColor Color,FString TextureName)
{
	FEnhancedScenePlan MyPlan;
	MyPlan.BackgroundColor = Color;
	MyPlan.TextColor = FColor::Green;
	MyPlan.ThemeName = FString("TestWall");
	MyPlan.TextureName=TextureName;
	if (MySceneBuilder)
	{
		MySceneBuilder->BuildScene(MyPlan, GetWorld());
	}
}

