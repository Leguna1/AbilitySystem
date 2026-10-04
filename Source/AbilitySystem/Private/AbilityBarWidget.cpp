#include "AbilityBarWidget.h"

#include "Ability.h"
#include "ActiveAbility.h"
#include "AbilityComponent.h"
#include "AbilitySlotWidget.h"
#include "Components/PanelWidget.h"

void UAbilityBarWidget::InitializeBar(UAbilityComponent* InAbilityComponent)
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->GrantedAbilitiesChangedEvent.RemoveDynamic(this, &UAbilityBarWidget::HandleGrantedAbilitiesChanged);
	}

	AbilityComponent = InAbilityComponent;

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->GrantedAbilitiesChangedEvent.AddDynamic(this, &UAbilityBarWidget::HandleGrantedAbilitiesChanged);
	}

	RebuildSlots();
}

void UAbilityBarWidget::HandleGrantedAbilitiesChanged()
{
	RebuildSlots();
}

void UAbilityBarWidget::RebuildSlots()
{
	ClearSlots();

	if (!IsValid(AbilityComponent) || !IsValid(SlotContainer) || !IsValid(SlotWidgetClass))
	{
		return;
	}

	for (const TSubclassOf<UAbility>& AbilityClass : AbilityComponent->GetGrantedAbilityClasses())
	{
		const UActiveAbility* ActiveDefaults = IsValid(AbilityClass)
			? Cast<UActiveAbility>(AbilityClass->GetDefaultObject())
			: nullptr;

		// Only player-activated abilities get a slot: passives fail the cast,
		// reactions and other triggered abilities have no input tag.
		if (!IsValid(ActiveDefaults) || !ActiveDefaults->GetActivationInputTag().IsValid())
		{
			continue;
		}

		UAbilitySlotWidget* AbilitySlot = CreateWidget<UAbilitySlotWidget>(this, SlotWidgetClass);

		if (!IsValid(AbilitySlot))
		{
			continue;
		}

		AbilitySlot->InitializeSlot(AbilityComponent, AbilityClass);
		SlotContainer->AddChild(AbilitySlot);
		SlotWidgets.Add(AbilitySlot);
	}
}

void UAbilityBarWidget::NativeDestruct()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->GrantedAbilitiesChangedEvent.RemoveDynamic(this, &UAbilityBarWidget::HandleGrantedAbilitiesChanged);
	}
	ClearSlots();
	Super::NativeDestruct();
}

void UAbilityBarWidget::ClearSlots()
{
	for (UAbilitySlotWidget* AbilitySlot : SlotWidgets)
	{
		if (IsValid(AbilitySlot))
		{
			AbilitySlot->RemoveFromParent();
		}
	}

	SlotWidgets.Reset();

	if (IsValid(SlotContainer))
	{
		SlotContainer->ClearChildren();
	}
}