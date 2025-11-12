// DojoStatsWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DojoStatsWidget.generated.h"

/**
 * Standalone stats display widget
 * All stat values are set manually from Blueprint or C++
 */
UCLASS()
class PROJECT_CANVAS_API UDojoStatsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ═══════════════════════════════════════════════════════
    // PUBLIC STAT VARIABLES (Set from Blueprint/C++)
    // ═══════════════════════════════════════════════════════
    
    /** Player Stats */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Player")
    int32 PlayerHits = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Player")
    int32 PlayerMaxCombo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Player")
    int32 PlayerCombo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Player")
    float PlayerDamage = 0.0f;

    /** Enemy Stats */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Enemy")
    int32 EnemyHits = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Enemy")
    int32 EnemyMaxCombo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Enemy")
    int32 EnemyCombo = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Enemy")
    float EnemyDamage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    FName LevelName;
    // ═══════════════════════════════════════════════════════
    // PUBLIC FUNCTIONS
    // ═══════════════════════════════════════════════════════
    
    /** Manually update all stat displays */
    UFUNCTION(BlueprintCallable, Category = "Stats")
    void UpdateStats();

    /** Reset all stats to zero */
    UFUNCTION(BlueprintCallable, Category = "Stats")
    void ResetStats();

    /** Update stats with new values (Blueprint callable) */
    UFUNCTION(BlueprintCallable, Category = "Stats")
    void UpdateStatsWithValues(
        int32 NewPlayerHits,
        int32 NewPlayerMaxCombo,
        int32 NewPlayerCombo,
        float NewPlayerDamage,
        int32 NewEnemyHits,
        int32 NewEnemyMaxCombo,
        int32 NewEnemyCombo,
        float NewEnemyDamage
    );

protected:
    // ═══════════════════════════════════════════════════════
    // UI TEXT BLOCKS (Bind in UMG Designer)
    // ═══════════════════════════════════════════════════════
    
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

    // ═══════════════════════════════════════════════════════
    // OPTIONAL BUTTONS
    // ═══════════════════════════════════════════════════════
    
    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* ButtonReset;

    UPROPERTY(meta = (BindWidgetOptional))
    class UButton* ButtonExit;

    UFUNCTION()
    void OnResetClicked();

    UFUNCTION()
    void OnExitClicked();
};
