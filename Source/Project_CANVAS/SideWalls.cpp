// Fill out your copyright notice in the Description page of Project Settings.


#include "SideWalls.h"


// Sets default values
ASideWalls::ASideWalls()
{
	Tags.Add("Side.Wall");
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void ASideWalls::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ASideWalls::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

