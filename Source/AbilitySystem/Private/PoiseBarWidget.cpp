#include "PoiseBarWidget.h"

#include "CombatantComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"

void UPoiseBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// No damage trail: the bar already shows the drop and eases the recovery itself.
	FUIBarMaterialSettings Settings = BarSettings;
	Settings.TrailDuration = 0.0f;
	BarMaterial.Initialize(BarFill, Settings);
	ApplyPercent();

	if (!IsValid(CombatantComponent) && bFindCombatantAutomatically)
	{
		if (const APawn* Pawn = GetOwningPlayerPawn())
		{
			SetCombatantComponent(Pawn->FindComponentByClass<UCombatantComponent>());
		}
	}

	if (!IsValid(CombatantComponent))
	{
		SetBarVisible(!bHideWhenFull);
	}
}

void UPoiseBarWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UPoiseBarWidget::SetCombatantComponent(UCombatantComponent* NewCombatantComponent)
{
	if (NewCombatantComponent == CombatantComponent)
	{
		return;
	}

	Unbind();
	CombatantComponent = NewCombatantComponent;
	Bind();

	DisplayedPercent = IsValid(CombatantComponent) ? CombatantComponent->GetPoisePercent() : 1.0f;
	BrokenHoldRemaining = 0.0f;
	FullTime = HideDelay;

	ApplyPercent();

	SetBarVisible(!bHideWhenFull || DisplayedPercent < 1.0f);
}

void UPoiseBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(CombatantComponent) || (!IsValid(PoiseBar) && !BarMaterial.IsValid()))
	{
		return;
	}

	if (BrokenHoldRemaining > 0.0f)
	{
		BrokenHoldRemaining -= InDeltaTime;
		return;
	}

	// Drops are instant, recovery eases in.
	const float ActualPercent = CombatantComponent->GetPoisePercent();

	DisplayedPercent = ActualPercent < DisplayedPercent
		? ActualPercent
		: FMath::FInterpTo(DisplayedPercent, ActualPercent, InDeltaTime, FillInterpSpeed);

	if (ActualPercent >= 1.0f && DisplayedPercent > 0.995f)
	{
		DisplayedPercent = 1.0f;
	}

	ApplyPercent();

	if (!bHideWhenFull)
	{
		return;
	}

	if (DisplayedPercent < 1.0f)
	{
		FullTime = 0.0f;
		return;
	}

	if (FullTime < HideDelay)
	{
		FullTime += InDeltaTime;

		if (FullTime >= HideDelay)
		{
			SetBarVisible(false);
		}
	}
}

void UPoiseBarWidget::HandlePoiseDamaged(const float Poise, const float MaxPoise, const bool bBroken)
{
	DisplayedPercent = MaxPoise > 0.0f ? FMath::Clamp(Poise / MaxPoise, 0.0f, 1.0f) : 0.0f;
	FullTime = 0.0f;

	ApplyPercent();

	SetBarVisible(true);

	if (bBroken)
	{
		BrokenHoldRemaining = BrokenHoldTime;
		OnPoiseBroken();
	}
	else
	{
		OnPoiseDamaged(DisplayedPercent);
	}
}

void UPoiseBarWidget::Bind()
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnPoiseDamaged.AddUniqueDynamic(this, &UPoiseBarWidget::HandlePoiseDamaged);
	}
}

void UPoiseBarWidget::Unbind()
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnPoiseDamaged.RemoveDynamic(this, &UPoiseBarWidget::HandlePoiseDamaged);
	}
}

void UPoiseBarWidget::SetBarVisible(const bool bVisible)
{
	// Hidden, not collapsed: the nameplate keeps its layout when the bar comes and goes.
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
}

void UPoiseBarWidget::ApplyPercent()
{
	if (IsValid(PoiseBar))
	{
		PoiseBar->SetPercent(DisplayedPercent);
	}

	// The bar eases its own recovery, so the material just follows.
	BarMaterial.SetPercent(DisplayedPercent, true);
}
