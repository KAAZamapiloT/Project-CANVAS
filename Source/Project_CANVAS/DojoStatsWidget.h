// DojoStatsWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DojoStatsWidget.generated.h"

/**
 * Widget that displays Dojo Mode stats
 */
UCLASS()
class PROJECT_CANVAS_API UDojoStatsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	/** Cached reference to Dojo Game Mode */
	UPROPERTY()
	class ADojoGameMode* DojoMode;

	/** Text blocks (bind in UMG Designer) */
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextPlayerHits;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextPlayerMaxCombo;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextPlayerCombo;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextPlayerDamage;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextEnemyHits;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextEnemyMaxCombo;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextEnemyCombo;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextEnemyDamage;

	/** Update all stat displays */
	UFUNCTION()
	void UpdateStats();

	/** Button callbacks */
	UPROPERTY(meta = (BindWidget))
	class UButton* ButtonReset;

	UPROPERTY(meta = (BindWidget))
	class UButton* ButtonExit;

	UFUNCTION()
	void OnResetClicked();

	UFUNCTION()
	void OnExitClicked();
};
