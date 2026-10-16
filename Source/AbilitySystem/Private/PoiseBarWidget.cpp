#include "PoiseBarWidget.h"

#include "CombatantComponent.h"
#include "Components/ProgressBar.h"
#include "GameFramework/Pawn.h"

void UPoiseBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

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

	if (IsValid(PoiseBar))
	{
		PoiseBar->SetPercent(DisplayedPercent);
	}

	SetBarVisible(!bHideWhenFull || DisplayedPercent < 1.0f);
}

void UPoiseBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!IsValid(CombatantComponent) || !IsValid(PoiseBar))
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

	PoiseBar->SetPercent(DisplayedPercent);

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

	if (IsValid(PoiseBar))
	{
		PoiseBar->SetPercent(DisplayedPercent);
	}

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
