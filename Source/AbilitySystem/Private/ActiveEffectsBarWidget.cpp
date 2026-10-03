#include "ActiveEffectsBarWidget.h"

#include "AbilityComponent.h"
#include "ActiveEffectWidget.h"
#include "Components/PanelWidget.h"
#include "TimerManager.h"

void UActiveEffectsBarWidget::InitializeEffectsBar(UAbilityComponent* InAbilityComponent)
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->ModifiersChangedEvent.RemoveDynamic(this, &UActiveEffectsBarWidget::HandleModifiersChanged);
	}

	AbilityComponent = InAbilityComponent;

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->ModifiersChangedEvent.AddDynamic(this, &UActiveEffectsBarWidget::HandleModifiersChanged);
	}

	SyncEffects();
}

void UActiveEffectsBarWidget::NativeDestruct()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->ModifiersChangedEvent.RemoveDynamic(this, &UActiveEffectsBarWidget::HandleModifiersChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	Super::NativeDestruct();
}

void UActiveEffectsBarWidget::HandleModifiersChanged()
{
	SyncEffects();
}

void UActiveEffectsBarWidget::SyncEffects()
{
	TArray<FActiveEffectInfo> Effects;

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->GetActiveEffects(Effects);
	}

	// Remove widgets whose effect has ended.
	for (int32 Index = EffectWidgets.Num() - 1; Index >= 0; --Index)
	{
		UActiveEffectWidget* EffectWidget = EffectWidgets[Index];

		const bool bStillActive = IsValid(EffectWidget) && Effects.ContainsByPredicate([EffectWidget](const FActiveEffectInfo& Effect)
		{
			return EffectWidget->Matches(Effect);
		});

		if (!bStillActive)
		{
			if (IsValid(EffectWidget))
			{
				EffectWidget->RemoveFromParent();
			}

			EffectWidgets.RemoveAt(Index);
		}
	}

	bool bAnyTimed = false;

	// Update existing widgets; add widgets for new effects.
	for (const FActiveEffectInfo& Effect : Effects)
	{
		bAnyTimed |= Effect.RemainingTime >= 0.0f;

		const TObjectPtr<UActiveEffectWidget>* Existing = EffectWidgets.FindByPredicate([&Effect](const TObjectPtr<UActiveEffectWidget>& EffectWidget)
		{
			return IsValid(EffectWidget) && EffectWidget->Matches(Effect);
		});

		if (Existing)
		{
			(*Existing)->UpdateEffect(Effect);
			continue;
		}

		if (!IsValid(EffectContainer) || !EffectWidgetClass)
		{
			continue;
		}

		UActiveEffectWidget* EffectWidget = CreateWidget<UActiveEffectWidget>(this, EffectWidgetClass);

		if (!IsValid(EffectWidget))
		{
			continue;
		}

		EffectWidget->InitializeEffect(Effect);
		EffectContainer->AddChild(EffectWidget);
		EffectWidgets.Add(EffectWidget);
	}

	// Poll countdowns only while something is ticking down.
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();

	if (bAnyTimed && !TimerManager.IsTimerActive(UpdateTimerHandle))
	{
		TimerManager.SetTimer(UpdateTimerHandle, this, &UActiveEffectsBarWidget::SyncEffects, UpdateInterval, true);
	}
	else if (!bAnyTimed)
	{
		TimerManager.ClearTimer(UpdateTimerHandle);
	}
}