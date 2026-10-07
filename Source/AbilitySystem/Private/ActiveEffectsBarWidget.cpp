#include "ActiveEffectsBarWidget.h"

#include "ActiveEffectWidget.h"
#include "CombatantComponent.h"
#include "Components/PanelWidget.h"
#include "TimerManager.h"

void UActiveEffectsBarWidget::InitializeEffectsBar(UCombatantComponent* InCombatantComponent)
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnEffectsChanged.RemoveDynamic(this, &UActiveEffectsBarWidget::HandleEffectsChanged);
	}

	CombatantComponent = InCombatantComponent;

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnEffectsChanged.AddDynamic(this, &UActiveEffectsBarWidget::HandleEffectsChanged);
	}

	SyncEffects();
}

void UActiveEffectsBarWidget::NativeDestruct()
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnEffectsChanged.RemoveDynamic(this, &UActiveEffectsBarWidget::HandleEffectsChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}

	Super::NativeDestruct();
}

void UActiveEffectsBarWidget::HandleEffectsChanged()
{
	SyncEffects();
}

void UActiveEffectsBarWidget::SyncEffects()
{
	TArray<FActiveEffectInfo> Effects;

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->GetActiveEffects(Effects);
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