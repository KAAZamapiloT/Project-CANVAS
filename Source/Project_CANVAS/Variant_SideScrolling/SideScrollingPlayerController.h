// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInput/Public/InputAction.h"
#include "ScenePlan.h"
#include "SideScrollingPlayerController.generated.h"

class ASideScrollingCharacter;
class UInputMappingContext;
class UGenAISystem; 
class USceneBuilder; 
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
	
protected:

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
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** Character class to respawn when the possessed pawn is destroyed */
	UPROPERTY(EditAnywhere, Category="Respawn")
	TSubclassOf<ASideScrollingCharacter> CharacterClass;

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

    /**TOGGLOING USER PROMPT 
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TObjectPtr<UInputAction> TogglePromptAction;

	UPROPERTY()
	//TObjectPtr<UGenAISystem> MyGenAISystem;
//
	//UPROPERTY()
	//TObjectPtr<USceneBuilder> MySceneBuilder;
	// REFRENCE TO PROMPT WIDGET
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	TSubclassOf<UUserWidget> PromptWidgetClass;
private:
	UPROPERTY()
	TObjectPtr<UUserWidget> PromptWidgetInstance;

	//function when we press input key
//	void TogglePromptUI();

	//UFUNCTION()
	//void OnPromptSubmitted(const FString& PromptText);

	//UFUNCTION()
	//void OnThemeDataReady(const FEnhancedScenePlan& Plan);**/
};
