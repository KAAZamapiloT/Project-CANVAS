// DojoGameMode.cpp
#include "DojoGameMode.h"
#include "TimerManager.h"
#include"DojoHUD.h"

ADojoGameMode::ADojoGameMode()
{
    // Inherits all settings from SideScrollingGameMode
    HUDClass = ADojoHUD::StaticClass();
    UE_LOG(LogTemp, Warning, TEXT("🥋 Dojo Mode Initialized"));
}

void ADojoGameMode::BeginPlay()
{
    Super::BeginPlay(); // ✅ Call parent to setup base game rules
    
    UE_LOG(LogTemp, Warning, TEXT("🥋 Dojo Training Mode Active"));
    UE_LOG(LogTemp, Display, TEXT("- Health damage disabled"));
    UE_LOG(LogTemp, Display, TEXT("- Hit tracking enabled"));
}


void ADojoGameMode::RecordPlayerHit(float Damage)
{
    PlayerHitsLanded++;
    PlayerCurrentCombo++;
    TotalDamageDealt += Damage;

    if (PlayerCurrentCombo > PlayerComboMax)
    {
        PlayerComboMax = PlayerCurrentCombo;
        UE_LOG(LogTemp, Warning, TEXT("🔥 New player combo record: %d"), PlayerComboMax);
    }

    UE_LOG(LogTemp, Display, TEXT("💥 Player hit! Combo: %d | Damage: %.1f"), 
           PlayerCurrentCombo, Damage);

    GetWorld()->GetTimerManager().ClearTimer(PlayerComboResetTimer);
    GetWorld()->GetTimerManager().SetTimer(
        PlayerComboResetTimer,
        this,
        &ADojoGameMode::ResetPlayerCombo,
        ComboResetDelay,
        false
    );
}

void ADojoGameMode::RecordEnemyHit(float Damage)
{
    EnemyHitsLanded++;
    EnemyCurrentCombo++;
    TotalDamageTaken += Damage;
    if (EnemyCurrentCombo > EnemyComboMax)
    {
        EnemyComboMax = EnemyCurrentCombo;
    }

    UE_LOG(LogTemp, Display, TEXT("💔 Enemy hit! Combo: %d | Damage: %.1f"), 
           EnemyCurrentCombo, Damage);

    GetWorld()->GetTimerManager().ClearTimer(EnemyComboResetTimer);
    GetWorld()->GetTimerManager().SetTimer(
        EnemyComboResetTimer,
        this,
        &ADojoGameMode::ResetEnemyCombo,
        ComboResetDelay,
        false
    );
}

void ADojoGameMode::ResetPlayerCombo()
{
    if (PlayerCurrentCombo > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("⏱️ Player combo ended at %d hits"), PlayerCurrentCombo);
    }
    PlayerCurrentCombo = 0;
}

void ADojoGameMode::ResetEnemyCombo()
{
    if (EnemyCurrentCombo > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("⏱️ Enemy combo ended at %d hits"), EnemyCurrentCombo);
    }
    EnemyCurrentCombo = 0;
}

void ADojoGameMode::ResetStats()
{
    PlayerHitsLanded = 0;
    EnemyHitsLanded = 0;
    PlayerComboMax = 0;
    EnemyComboMax = 0;
    PlayerCurrentCombo = 0;
    EnemyCurrentCombo = 0;
    TotalDamageDealt = 0.0f;  // ✅ ADD THIS
    TotalDamageTaken = 0.0f;  // ✅ ADD THIS
    GetWorld()->GetTimerManager().ClearTimer(PlayerComboResetTimer);
    GetWorld()->GetTimerManager().ClearTimer(EnemyComboResetTimer);

    UE_LOG(LogTemp, Warning, TEXT("🔄 Dojo stats reset"));
}
