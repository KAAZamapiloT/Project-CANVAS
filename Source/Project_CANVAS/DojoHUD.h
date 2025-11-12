// DojoHUD.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DojoHUD.generated.h"

/**
 * HUD for Dojo Training Mode
 */
UCLASS()
class PROJECT_CANVAS_API ADojoHUD : public AHUD
{
	GENERATED_BODY()

public:
	ADojoHUD();

protected:
	virtual void BeginPlay() override;

	/** Reference to the stats widget */
	UPROPERTY()
	class UUserWidget* StatsWidget;

	/** Widget class to spawn */
	UPROPERTY(EditDefaultsOnly, Category = "Dojo UI")
	TSubclassOf<UUserWidget> StatsWidgetClass;
};
