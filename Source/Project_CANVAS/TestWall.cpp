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
	FFileHelper::LoadFileToString(JsonString, *FilePath);

	FEnhancedScenePlan MyPlan = Parser->CreatePlan(JsonString);
   // FEnhancedScenePlan MyPlan;
	//MyPlan.BackgroundColor = FColor::Green;
	//MyPlan.TextColor = FColor::Green;
//	MyPlan.ThemeName = FString("TestWall");
	if (MySceneBuilder)
	{
		MySceneBuilder->BuildScene(MyPlan, GetWorld());
	}
	
}

// Called every frame
void ATestWall::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// just  a test function

void ATestWall::ChangeColor(FColor Color)
{
	FEnhancedScenePlan MyPlan;
	MyPlan.BackgroundColor = Color;
	MyPlan.TextColor = FColor::Green;
	MyPlan.ThemeName = FString("TestWall");
	if (MySceneBuilder)
	{
		MySceneBuilder->BuildScene(MyPlan, GetWorld());
	}
}

