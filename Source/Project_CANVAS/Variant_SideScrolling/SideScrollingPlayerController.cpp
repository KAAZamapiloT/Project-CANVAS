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
#include "SceneBuilder.h"
#include "EnhancedInputComponent.h"
#include "UObject/UObjectGlobals.h"
#include"SceneStateTracker.h"
#include "UObject/Class.h"

void ASideScrollingPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	
	Tags.Add("Player");
	// only spawn touch controls on local player controllers
	if (SVirtualJoystick::ShouldDisplayTouchInterface() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogProject_CANVAS, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void ASideScrollingPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent(); // CRITICAL: Call parent first!
	
	UE_LOG(LogTemp, Warning, TEXT("!!! SetupInputComponent START !!! InputComponent Valid? %s"), InputComponent ? TEXT("Yes") : TEXT("NO"));
	
	// Force creation if still null (shouldn't be needed but safety check)
	if (!InputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("SetupInputComponent: InputComponent is NULL after Super call! This is abnormal."));
		return;
	}

	// Bind the toggle prompt action here
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (TogglePromptAction)
		{
			UE_LOG(LogTemp, Warning, TEXT("SetupInputComponent: Binding TogglePromptAction..."));
			EnhancedInputComponent->BindAction(TogglePromptAction, ETriggerEvent::Triggered, this, &ASideScrollingPlayerController::TogglePromptUI);
			UE_LOG(LogTemp, Warning, TEXT("SetupInputComponent: TogglePromptAction BOUND successfully!"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SetupInputComponent: TogglePromptAction is NULL! Check Blueprint defaults."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SetupInputComponent: Failed to cast to EnhancedInputComponent!"));
	}

	// Add input mapping contexts - only for local player controllers
	if (IsLocalPlayerController())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetupInputComponent: IS LOCAL PLAYER CONTROLLER"));
		
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			// Add default mapping contexts
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				if (CurrentContext)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
					UE_LOG(LogTemp, Log, TEXT("SetupInputComponent: Added Default IMC: %s"), *GetNameSafe(CurrentContext));
				}
			}

			// Only add these IMCs if we're not using mobile touch input
			if (!SVirtualJoystick::ShouldDisplayTouchInterface())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					if (CurrentContext)
					{
						Subsystem->AddMappingContext(CurrentContext, 0);
						UE_LOG(LogTemp, Log, TEXT("SetupInputComponent: Added Mobile Excluded IMC: %s"), *GetNameSafe(CurrentContext));
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SetupInputComponent: Failed to get EnhancedInputLocalPlayerSubsystem!"));
		}
	}
}

void ASideScrollingPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	UE_LOG(LogTemp, Warning, TEXT("ASideScrollingPlayerController::OnPossess - Possessing Pawn: %s"), *GetNameSafe(InPawn));

	if (InPawn == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("OnPossess: InPawn is NULL!"));
		return;
	}

	// InputComponent setup should have been done in SetupInputComponent
	// We're just logging status here
	if (InputComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("OnPossess: InputComponent is valid!"));
		
		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		{
			UE_LOG(LogTemp, Log, TEXT("OnPossess: EnhancedInputComponent is valid!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OnPossess: InputComponent is STILL NULL! Something is very wrong."));
	}

	// Pawn destruction binding
	InPawn->OnDestroyed.AddDynamic(this, &ASideScrollingPlayerController::OnPawnDestroyed);
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

void ASideScrollingPlayerController::TogglePromptUI()
{
	// 1. Create once, bind once
	if (!PromptWidgetInstance)
	{
		PromptWidgetInstance = CreateWidget<UUserWidget>(this, PromptWidgetClass);
		if (!PromptWidgetInstance) return;
		PromptWidgetInstance->AddToViewport();

		// BIND ONLY HERE - inside the creation block
		FMulticastDelegateProperty* DispatcherProp = FindFieldChecked<FMulticastDelegateProperty>(
			PromptWidgetInstance->GetClass(), FName("OnPromptSubmitted"));
		if (DispatcherProp) {
			FScriptDelegate Delegate;
			Delegate.BindUFunction(this, FName("OnPromptSubmitted"));
			DispatcherProp->AddDelegate(Delegate, PromptWidgetInstance);
		}
	}

	// 2. Simply Toggle Visibility
	ESlateVisibility NewVisibility = PromptWidgetInstance->IsVisible() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	PromptWidgetInstance->SetVisibility(NewVisibility);
    
	// 3. Update Input Mode
	if (NewVisibility == ESlateVisibility::Visible) {
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
		UGameplayStatics::SetGamePaused(GetWorld(), true);
	} else {
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		UGameplayStatics::SetGamePaused(GetWorld(), false);
	}
}

void ASideScrollingPlayerController::OnPromptSubmitted(const FString& PromptText)
{
	FString UserPrompt = PromptText;
    
	UE_LOG(LogTemp, Warning, TEXT("PlayerController: Prompt submitted: %s"), *UserPrompt);
    
	// Get the Game Instance
	UGameInstance* GameInstance = GetGameInstance();
    
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController: GameInstance is NOT SceneStateTracker!"));
		return;
	}
    USceneStateTracker* Tracker=GameInstance->GetSubsystem<USceneStateTracker>();
	if (!Tracker||!Tracker->GenAISystem)
	{
		UE_LOG(LogTemp, Error, TEXT("PlayerController: GenAISystem is NULL!"));
		return;
	}
    
	// Call GenAI to process the prompt
	UE_LOG(LogTemp, Warning, TEXT("PlayerController: Calling GenAISystem->RequestSceneChange()"));
	Tracker->GenAISystem->RequestSceneChange(UserPrompt, GetWorld(),Tracker->HistoryManager);

	// Automatically close the UI after submitting
	TogglePromptUI();
}

void ASideScrollingPlayerController::ShowBounds(float Duration)
{
	USceneStateTracker* StateTracker = GetGameInstance()->GetSubsystem<USceneStateTracker>();
	if (StateTracker && StateTracker->LocationEngine)
	{
		StateTracker->LocationEngine->VisualizePlayableAreaBounds(Duration);
		ClientMessage(FString::Printf(TEXT("✅ Showing bounds for %.1f seconds"), Duration));
	}
	else
	{
		ClientMessage(TEXT("❌ LocationEngine not initialized!"));
	}
}

void ASideScrollingPlayerController::ShowLocations(float Duration)
{
	USceneStateTracker* StateTracker = Cast<USceneStateTracker>(GetGameInstance()->GetSubsystem<USceneStateTracker>());
	if (StateTracker && StateTracker->LocationEngine)
	{
		StateTracker->LocationEngine->VisualizeAllLocations(Duration);
		ClientMessage(TEXT("✅ Showing all spawn locations"));
	}
	else
	{
		ClientMessage(TEXT("❌ LocationEngine not found"));
	}
}


void ASideScrollingPlayerController::ListLocations()
{
	USceneStateTracker* StateTracker = Cast<USceneStateTracker>(GetGameInstance()->GetSubsystem<USceneStateTracker>());
	if (StateTracker && StateTracker->LocationEngine)
	{
		StateTracker->LocationEngine->PrintAllLocationData();
		ClientMessage(TEXT("✅ Location data printed to log (check Output Log window)"));
	}
	else
	{
		ClientMessage(TEXT("❌ LocationEngine not found"));
	}
}


void ASideScrollingPlayerController::ShowOccupancy()
{
	USceneStateTracker* StateTracker = Cast<USceneStateTracker>(GetGameInstance()->GetSubsystem<USceneStateTracker>());
	if (StateTracker && StateTracker->LocationEngine)
	{
		StateTracker->LocationEngine->PrintLocationsByStatus();
		ClientMessage(TEXT("✅ Occupancy status printed to log"));
	}
	else
	{
		ClientMessage(TEXT("❌ LocationEngine not found"));
	}
}

void ASideScrollingPlayerController::AuditTexture(FString ST)
{
	USceneStateTracker* Tracker =GetGameInstance()->GetSubsystem<USceneStateTracker>();
	Tracker->AssetIndexer->AuditTexture(ST);
}
