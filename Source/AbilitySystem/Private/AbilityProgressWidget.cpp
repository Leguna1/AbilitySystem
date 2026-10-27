#include "AbilityProgressWidget.h"

#include "Ability.h"
#include "ActiveAbility.h"
#include "AbilityComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "TimerManager.h"

void UAbilityProgressWidget::InitializeProgress(UAbilityComponent* InAbilityComponent)
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityActivatedEvent.RemoveDynamic(this, &UAbilityProgressWidget::HandleAbilityActivated);
		AbilityComponent->AbilityEndedEvent.RemoveDynamic(this, &UAbilityProgressWidget::HandleAbilityEnded);
	}

	AbilityComponent = InAbilityComponent;

	if (!IsValid(AbilityComponent))
	{
		Hide();
		return;
	}

	AbilityComponent->AbilityActivatedEvent.AddDynamic(this, &UAbilityProgressWidget::HandleAbilityActivated);
	AbilityComponent->AbilityEndedEvent.AddDynamic(this, &UAbilityProgressWidget::HandleAbilityEnded);

	// Handle an ability that is already active at bind time.
	EvaluateActiveAbility();
}

void UAbilityProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// The bar follows the ability's own progress, so no damage trail.
	FUIBarMaterialSettings Settings = BarSettings;
	Settings.TrailDuration = 0.0f;
	BarMaterial.Initialize(BarFill, Settings);

	if (bManageVisibility && !bShowing)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UAbilityProgressWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (ShowElapsed < 0.0f)
	{
		return;
	}

	ShowElapsed += InDeltaTime;
	const float Alpha = ShowDuration > 0.0f ? FMath::Clamp(ShowElapsed / ShowDuration, 0.0f, 1.0f) : 1.0f;
	const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, Alpha, 2.0f);

	SetRenderOpacity(Eased);
	SetRenderTranslation(FVector2D(0.0f, (1.0f - Eased) * ShowSlideDistance));

	if (Alpha >= 1.0f)
	{
		ShowElapsed = -1.0f;
	}
}

void UAbilityProgressWidget::NativeDestruct()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityActivatedEvent.RemoveDynamic(this, &UAbilityProgressWidget::HandleAbilityActivated);
		AbilityComponent->AbilityEndedEvent.RemoveDynamic(this, &UAbilityProgressWidget::HandleAbilityEnded);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FillTimerHandle);
	}

	Super::NativeDestruct();
}

void UAbilityProgressWidget::HandleAbilityActivated(FGameplayTag AbilityId, UActiveAbility* Ability)
{
	EvaluateActiveAbility();
}

void UAbilityProgressWidget::HandleAbilityEnded(FGameplayTag AbilityId, UActiveAbility* Ability, EAbilityEndReason EndReason)
{
	// The active ability is changing; re-evaluate against whatever is active now
	// (may be nothing, or an immediate replacement).
	EvaluateActiveAbility();
}

void UAbilityProgressWidget::EvaluateActiveAbility()
{
	if (!IsValid(AbilityComponent))
	{
		Hide();
		return;
	}

	UActiveAbility* Active = AbilityComponent->GetActiveAbility();
	if (!IsValid(Active) ||
	Active->GetAbilityStatus() != EAbilityStatus::Active ||
	!Active->GetClass()->ImplementsInterface(UAbilityProgressProvider::StaticClass()))
	{
		Hide();
		return;
	}

	const FAbilityProgress Progress = IAbilityProgressProvider::Execute_GetAbilityProgress(Active);
	if (Progress.Kind == EAbilityProgressKind::None)
	{
		// Provider exists but has nothing to show yet (e.g. still in windup).
		// Keep the timer running so it appears the moment Kind flips.
		if (bShowing)
		{
			Hide();
		}

		// Start a light poll so we can catch the transition into a shown state.
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				FillTimerHandle, this, &UAbilityProgressWidget::TickFill, UpdateInterval, true);
		}
		return;
	}

	Show(Progress.Kind);
}

void UAbilityProgressWidget::TickFill()
{
	if (!IsValid(AbilityComponent))
	{
		Hide();
		return;
	}

	UActiveAbility* Active = AbilityComponent->GetActiveAbility();
	
	if (!IsValid(Active) ||
	Active->GetAbilityStatus() != EAbilityStatus::Active ||
	!Active->GetClass()->ImplementsInterface(UAbilityProgressProvider::StaticClass()))
	{
		Hide();
		return;
	}

	const FAbilityProgress Progress = IAbilityProgressProvider::Execute_GetAbilityProgress(Active);

	if (Progress.Kind == EAbilityProgressKind::None)
	{
		if (bShowing)
		{
			Hide();
		}
		return;
	}

	if (!bShowing)
	{
		Show(Progress.Kind);
	}

	const FText Formatted = FormatProgressText(Progress);
	UpdateKitVisuals(Progress, Formatted);
	OnProgressUpdated(Progress, Formatted);
}

void UAbilityProgressWidget::Show(EAbilityProgressKind Kind)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FillTimerHandle, this, &UAbilityProgressWidget::TickFill, UpdateInterval, true);
	}

	if (!bShowing)
	{
		bShowing = true;

		if (bManageVisibility)
		{
			SetVisibility(ESlateVisibility::HitTestInvisible);
			SetRenderOpacity(0.0f);
			ShowElapsed = 0.0f;
		}

		OnProgressShown(Kind);
	}
}

void UAbilityProgressWidget::Hide()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FillTimerHandle);
	}

	if (bShowing)
	{
		bShowing = false;

		if (bManageVisibility)
		{
			ShowElapsed = -1.0f;
			SetVisibility(ESlateVisibility::Collapsed);
		}

		OnProgressHidden();
	}
}

float UAbilityProgressWidget::GetBarPercent(const FAbilityProgress& Progress) const
{
	if (Progress.Kind == EAbilityProgressKind::Count)
	{
		const float Done = Progress.Max > 0
			? static_cast<float>(Progress.Current) / static_cast<float>(Progress.Max)
			: Progress.Normalized;

		return FMath::Clamp(bInvertCount ? Done : 1.0f - Done, 0.0f, 1.0f);
	}

	return FMath::Clamp(Progress.Normalized, 0.0f, 1.0f);
}

void UAbilityProgressWidget::UpdateKitVisuals(const FAbilityProgress& Progress, const FText& Formatted)
{
	BarMaterial.SetPercent(GetBarPercent(Progress), true);

	if (!IsValid(ProgressLabel))
	{
		return;
	}

	const UActiveAbility* Active = IsValid(AbilityComponent) ? AbilityComponent->GetActiveAbility() : nullptr;

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Name"), IsValid(Active) ? Active->GetDisplayName() : FText::GetEmpty());
	Arguments.Add(TEXT("Progress"), Formatted);

	ProgressLabel->SetText(FText::Format(ProgressTextFormat, Arguments));
}

FText UAbilityProgressWidget::FormatProgressText(const FAbilityProgress& Progress)
{
	switch (Progress.Kind)
	{
	case EAbilityProgressKind::Charge:
	{
		const int32 Percent = FMath::RoundToInt(Progress.Normalized * 100.0f);
		return FText::FromString(FString::Printf(TEXT("%d%%"), Percent));
	}
	case EAbilityProgressKind::Count:
	{
		return FText::FromString(FString::Printf(TEXT("%d/%d"), Progress.Current, Progress.Max));
	}
	default:
		return FText::GetEmpty();
	}
}