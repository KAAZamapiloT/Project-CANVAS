// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SceneBuilder.h"
#include "ScenePlan.h"

//#include"JsonParser.h"
#include "TestWall.generated.h"

UCLASS()
class PROJECT_CANVAS_API ATestWall : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ATestWall();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
private:
	
public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;



};

