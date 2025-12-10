// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInput/Public/InputAction.h"

#include "SideScrollingPlayerController.generated.h"

class ASideScrollingCharacter;
class UInputMappingContext;
/**
 *  A simple Side Scrolling Player Controller
 *  Manages input mappings
 *  Respawns the player pawn at the player start if it is destroyed
 */
UCLASS(abstract)
class ASideScrollingPlayerController : public APlayerController
{
	GENERATED_BODY()


public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite,Category = "UI")
	TSubclassOf<UUserWidget> PromptWidgetClass;
protected:
	UPROPERTY(EditAnywhere, Category="Input|Input Actions") // Changed category slightly
	TObjectPtr<UInputAction> TogglePromptAction;
	UPROPERTY()
    TObjectPtr<UUserWidget> PromptWidgetInstance;
	
	/** Input mapping context for this player */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Character class to respawn when the possessed pawn is destroyed */
	UPROPERTY(EditAnywhere, Category="Respawn")
	TSubclassOf<ASideScrollingCharacter> CharacterClass;
private:
	void TogglePromptUI();
	UFUNCTION()
	void OnPromptSubmitted(const FString& PromptText);
protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn) override;

	/** Called if the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);
   
public:
	// Debug commands for LocationEngine
	/** Shows playable area bounds with colored boxes */
	UFUNCTION(Exec)
	void ShowBounds(float Duration = 10.0f);
    
	/** Shows all spawn locations with debug spheres */
	UFUNCTION(Exec)
	void ShowLocations(float Duration = 10.0f);
    
	/** Prints location database to log */
	UFUNCTION(Exec)
	void ListLocations();
    
	/** Prints occupancy status of all locations */
	UFUNCTION(Exec)
	void ShowOccupancy();
	
	UFUNCTION(Exec)
	void AuditTexture(FString ST);

};
