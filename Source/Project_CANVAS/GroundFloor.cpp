// Fill out your copyright notice in the Description page of Project Settings.


#include "GroundFloor.h"


// Sets default values
AGroundFloor::AGroundFloor()
{

	Tags.Add("Ground.Floor");
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

// Called when the game starts or when spawned
void AGroundFloor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AGroundFloor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

