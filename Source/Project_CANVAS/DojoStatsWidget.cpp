// DojoStatsWidget.cpp
#include "DojoStatsWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

void UDojoStatsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Bind button callbacks if they exist
    if (ButtonReset)
    {
        ButtonReset->OnClicked.AddDynamic(this, &UDojoStatsWidget::OnResetClicked);
    }

    if (ButtonExit)
    {
        ButtonExit->OnClicked.AddDynamic(this, &UDojoStatsWidget::OnExitClicked);
    }

    // Initial update
    UpdateStats();

    UE_LOG(LogTemp, Log, TEXT("✅ DojoStatsWidget initialized (Standalone mode)"));
}

void UDojoStatsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Auto-update display every frame
    UpdateStats();
}

void UDojoStatsWidget::UpdateStats()
{
    // Update Player Stats
    if (TextPlayerHits)
    {
        TextPlayerHits->SetText(FText::Format(
            FText::FromString("Hits: {0}"),
            FText::AsNumber(PlayerHits)
        ));
    }

    if (TextPlayerMaxCombo)
    {
        TextPlayerMaxCombo->SetText(FText::Format(
            FText::FromString("Max Combo: {0}"),
            FText::AsNumber(PlayerMaxCombo)
        ));
    }

    if (TextPlayerCombo)
    {
        TextPlayerCombo->SetText(FText::Format(
            FText::FromString("Combo: {0}"),
            FText::AsNumber(PlayerCombo)
        ));
    }

    if (TextPlayerDamage)
    {
        TextPlayerDamage->SetText(FText::Format(
            FText::FromString("Damage: {0}"),
            FText::AsNumber(FMath::RoundToInt(PlayerDamage))
        ));
    }

    // Update Enemy Stats
    if (TextEnemyHits)
    {
        TextEnemyHits->SetText(FText::Format(
            FText::FromString("Hits: {0}"),
            FText::AsNumber(EnemyHits)
        ));
    }

    if (TextEnemyMaxCombo)
    {
        TextEnemyMaxCombo->SetText(FText::Format(
            FText::FromString("Max Combo: {0}"),
            FText::AsNumber(EnemyMaxCombo)
        ));
    }

    if (TextEnemyCombo)
    {
        TextEnemyCombo->SetText(FText::Format(
            FText::FromString("Combo: {0}"),
            FText::AsNumber(EnemyCombo)
        ));
    }

    if (TextEnemyDamage)
    {
        TextEnemyDamage->SetText(FText::Format(
            FText::FromString("Damage: {0}"),
            FText::AsNumber(FMath::RoundToInt(EnemyDamage))
        ));
    }
}

void UDojoStatsWidget::ResetStats()
{
    PlayerHits = 0;
    PlayerMaxCombo = 0;
    PlayerCombo = 0;
    PlayerDamage = 0.0f;

    EnemyHits = 0;
    EnemyMaxCombo = 0;
    EnemyCombo = 0;
    EnemyDamage = 0.0f;

    UpdateStats();

    UE_LOG(LogTemp, Warning, TEXT("🔄 Stats reset"));
}

void UDojoStatsWidget::OnResetClicked()
{
    ResetStats();
}

void UDojoStatsWidget::OnExitClicked()
{
    if (LevelName.IsNone())
    {
        UGameplayStatics::OpenLevel(GetWorld(), FName("MainMenu"));
    }else
    {
        UGameplayStatics::OpenLevel(GetWorld(), LevelName);
    }
    // Return to main menu or previous level
   
}
void UDojoStatsWidget::UpdateStatsWithValues(
    int32 NewPlayerHits,
    int32 NewPlayerMaxCombo,
    int32 NewPlayerCombo,
    float NewPlayerDamage,
    int32 NewEnemyHits,
    int32 NewEnemyMaxCombo,
    int32 NewEnemyCombo,
    float NewEnemyDamage)
{
    // Update member variables
    PlayerHits = NewPlayerHits;
    PlayerMaxCombo = NewPlayerMaxCombo;
    PlayerCombo = NewPlayerCombo;
    PlayerDamage = NewPlayerDamage;
    
    EnemyHits = NewEnemyHits;
    EnemyMaxCombo = NewEnemyMaxCombo;
    EnemyCombo = NewEnemyCombo;
    EnemyDamage = NewEnemyDamage;
    
    // Update UI display
    UpdateStats();
}
