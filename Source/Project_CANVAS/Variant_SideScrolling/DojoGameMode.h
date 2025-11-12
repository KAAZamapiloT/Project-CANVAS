// DojoGameMode.h
#pragma once

#include "CoreMinimal.h"
#include "SideScrollingGameMode.h" // ✅ Inherit from your base mode
#include "DojoGameMode.generated.h"

/**
 * Training Mode (Dojo)
 * - Inherits all sidescroller rules
 * - Adds stats tracking
 * - Disables death
 */
UCLASS()
class PROJECT_CANVAS_API ADojoGameMode : public ASideScrollingGameMode // ✅ Changed parent
{
	GENERATED_BODY()

public:
	ADojoGameMode();

	/** Override BeginPlay to add training setup */
	virtual void BeginPlay() override;



	/** Combat statistics (dojo-specific) */
	UPROPERTY(BlueprintReadOnly, Category = "Dojo Stats")
	int32 PlayerHitsLanded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dojo Stats")
	int32 EnemyHitsLanded = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dojo Stats")
	int32 PlayerComboMax = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dojo Stats")
	int32 EnemyComboMax = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dojo Stats")
	int32 PlayerCurrentCombo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dojo Stats")
	int32 EnemyCurrentCombo = 0;
	// ✅ ADD THESE MISSING PROPERTIES
	UPROPERTY(BlueprintReadOnly, Category = "Dojo Stats")
	float TotalDamageDealt = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Dojo Stats")
	float TotalDamageTaken = 0.0f;
	/** Dojo-specific functions */
	UFUNCTION(BlueprintCallable, Category = "Dojo")
	void RecordPlayerHit(float Damage);

	UFUNCTION(BlueprintCallable, Category = "Dojo")
	void RecordEnemyHit(float Damage);

	UFUNCTION(BlueprintCallable, Category = "Dojo")
	void ResetStats();

protected:
	/** HUD class for Dojo Mode */
	UPROPERTY(EditDefaultsOnly, Category = "Dojo UI")
	TSubclassOf<class ADojoHUD> DojoHUDClass;
	
	void ResetPlayerCombo();
	void ResetEnemyCombo();

	FTimerHandle PlayerComboResetTimer;
	FTimerHandle EnemyComboResetTimer;

	UPROPERTY(EditAnywhere, Category = "Dojo Settings")
	float ComboResetDelay = 2.0f;
};
