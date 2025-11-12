// DojoStatsWidget.cpp
#include "DojoStatsWidget.h"
#include "DojoGameMode.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

void UDojoStatsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Get reference to Dojo Game Mode
    DojoMode = Cast<ADojoGameMode>(GetWorld()->GetAuthGameMode());
    if (!DojoMode)
    {
        UE_LOG(LogTemp, Error, TEXT("❌ DojoStatsWidget: Not in Dojo Mode!"));
        return;
    }

    // Bind button clicks
    if (ButtonReset)
    {
        ButtonReset->OnClicked.AddDynamic(this, &UDojoStatsWidget::OnResetClicked);
    }

    if (ButtonExit)
    {
        ButtonExit->OnClicked.AddDynamic(this, &UDojoStatsWidget::OnExitClicked);
    }

    UE_LOG(LogTemp, Display, TEXT("✅ Dojo Stats Widget initialized"));
}

void UDojoStatsWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    
    if (DojoMode)
    {
        UpdateStats();
    }
}

void UDojoStatsWidget::UpdateStats()
{
    if (!DojoMode) return;

    // Update Player Stats
    if (TextPlayerHits)
        TextPlayerHits->SetText(FText::AsNumber(DojoMode->PlayerHitsLanded));

    if (TextPlayerMaxCombo)
        TextPlayerMaxCombo->SetText(FText::AsNumber(DojoMode->PlayerComboMax));

    if (TextPlayerCombo)
        TextPlayerCombo->SetText(FText::AsNumber(DojoMode->PlayerCurrentCombo));

    if (TextPlayerDamage)
        TextPlayerDamage->SetText(FText::AsNumber(FMath::RoundToInt(DojoMode->TotalDamageDealt)));

    // Update Enemy Stats
    if (TextEnemyHits)
        TextEnemyHits->SetText(FText::AsNumber(DojoMode->EnemyHitsLanded));

    if (TextEnemyMaxCombo)
        TextEnemyMaxCombo->SetText(FText::AsNumber(DojoMode->EnemyComboMax));

    if (TextEnemyCombo)
        TextEnemyCombo->SetText(FText::AsNumber(DojoMode->EnemyCurrentCombo));

    if (TextEnemyDamage)
        TextEnemyDamage->SetText(FText::AsNumber(FMath::RoundToInt(DojoMode->TotalDamageTaken)));
}

void UDojoStatsWidget::OnResetClicked()
{
    if (DojoMode)
    {
        DojoMode->ResetStats();
        UE_LOG(LogTemp, Warning, TEXT("🔄 Stats reset"));
    }
}

void UDojoStatsWidget::OnExitClicked()
{
    UGameplayStatics::OpenLevel(GetWorld(), FName("MainMenu"));
}
