// DojoHUD.cpp
#include "DojoHUD.h"
#include "Blueprint/UserWidget.h"

ADojoHUD::ADojoHUD()
{
}

void ADojoHUD::BeginPlay()
{
	Super::BeginPlay();

	if (StatsWidgetClass)
	{
		StatsWidget = CreateWidget<UUserWidget>(GetWorld(), StatsWidgetClass);
		if (StatsWidget)
		{
			StatsWidget->AddToViewport();
			UE_LOG(LogTemp, Display, TEXT("🥋 Dojo HUD created"));
		}
	}
}
