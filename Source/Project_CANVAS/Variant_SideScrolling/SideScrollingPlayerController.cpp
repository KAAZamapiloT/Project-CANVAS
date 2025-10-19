// Copyright Epic Games, Inc. All Rights Reserved.


#include "SideScrollingPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "SideScrollingCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Project_CANVAS.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Components/EditableText.h"
#include "GenAISystem.h"
#include"SceneBuilder.h"
#include "EnhancedInputComponent.h"

#include "UObject/UObjectGlobals.h" 
#include "UObject/Class.h"
void ASideScrollingPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	
	// only spawn touch controls on local player controllers
	if (SVirtualJoystick::ShouldDisplayTouchInterface() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogProject_CANVAS, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
	
}

void ASideScrollingPlayerController::SetupInputComponent()
{
	UE_LOG(LogTemp, Warning, TEXT("!!! SetupInputComponent START !!! InputComponent Valid? %s"), InputComponent ? TEXT("Yes") : TEXT("NO"));
	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		UE_LOG(LogTemp,Warning,TEXT(" IS LOCAL PLAYER CONTROLER BRANCH IS GETTIG ENTERED 	"))
		// add the input mapping context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!SVirtualJoystick::ShouldDisplayTouchInterface())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}

				
			}
			
		}
	
	}
}

void ASideScrollingPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Warning, TEXT("ASideScrollingPlayerController::OnPossess - Possessing Pawn: %s"), *GetNameSafe(InPawn));

	// Make sure we have a valid Pawn
	if (InPawn == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("OnPossess: InPawn is NULL!"));
		return;
	}

	// Get the Enhanced Input Component - It SHOULD exist now
    // NOTE: We need to ensure the Input Component is created if it wasn't.
    // APlayerController::InitInputSystem() usually does this. Let's ensure it runs.
    InitInputSystem(); // Explicitly ensure input system is initialized

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// --- Bind the Toggle Prompt Action ---
		

        // --- Add Input Mapping Context ---
        // This also needs to happen after input system is ready
        if (IsLocalPlayerController())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
            {
                // It's generally safer to clear before adding, though BeginPlay might also do this.
                // Subsystem->ClearAllMappings(); 
                for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
                {
                    Subsystem->AddMappingContext(CurrentContext, 0);
                     UE_LOG(LogTemp, Log, TEXT("OnPossess: Added IMC %s"), *GetNameSafe(CurrentContext));
                }
                if (!SVirtualJoystick::ShouldDisplayTouchInterface())
                {
                    for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
                    {
                        Subsystem->AddMappingContext(CurrentContext, 0);
                        UE_LOG(LogTemp, Log, TEXT("OnPossess: Added Mobile Excluded IMC %s"), *GetNameSafe(CurrentContext));
                    }
                }
            }
        }

	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OnPossess: Failed to Cast InputComponent to UEnhancedInputComponent!"));
        // If InputComponent itself is null, log that too
        if(InputComponent == nullptr)
        {
             UE_LOG(LogTemp, Error, TEXT("OnPossess: InputComponent is NULL even after InitInputSystem!"));
        }
	}


	// --- Existing OnDestroyed Binding ---
	if (InPawn) // Double check after potential logging
	{
		InPawn->OnDestroyed.AddDynamic(this, &ASideScrollingPlayerController::OnPawnDestroyed);
	}
}

void ASideScrollingPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	// find the player start
	TArray<AActor*> ActorList;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), ActorList);

	if (ActorList.Num() > 0)
	{
		// spawn a character at the player start
		const FTransform SpawnTransform = ActorList[0]->GetActorTransform();

		if (ASideScrollingCharacter* RespawnedCharacter = GetWorld()->SpawnActor<ASideScrollingCharacter>(CharacterClass, SpawnTransform))
		{
			// possess the character
			Possess(RespawnedCharacter);
		}
	}
}


