#include "AbilityProgressWidget.h"

#include "Ability.h"
#include "AbilityComponent.h"
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

void UAbilityProgressWidget::HandleAbilityActivated(FGameplayTag AbilityId, UAbility* Ability)
{
	EvaluateActiveAbility();
}

void UAbilityProgressWidget::HandleAbilityEnded(FGameplayTag AbilityId, UAbility* Ability, EAbilityEndReason EndReason)
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

	UAbility* Active = AbilityComponent->GetActiveAbility();
	if (!IsValid(Active) || !Active->GetClass()->ImplementsInterface(UAbilityProgressProvider::StaticClass()))
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

	UAbility* Active = AbilityComponent->GetActiveAbility();
	if (!IsValid(Active) || !Active->GetClass()->ImplementsInterface(UAbilityProgressProvider::StaticClass()))
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

	OnProgressUpdated(Progress, FormatProgressText(Progress));
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
		OnProgressHidden();
	}
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