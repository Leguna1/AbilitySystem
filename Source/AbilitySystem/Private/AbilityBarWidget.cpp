#include "AbilityBarWidget.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "AbilitySlotWidget.h"
#include "Components/PanelWidget.h"

void UAbilityBarWidget::InitializeBar(UAbilityComponent* InAbilityComponent)
{
	AbilityComponent = InAbilityComponent;
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
		if (!IsValid(AbilityClass))
		{
			continue;
		}

		UAbilitySlotWidget* AbilitySlot = CreateWidget<UAbilitySlotWidget>(this, SlotWidgetClass);
		if (!IsValid(Slot))
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
	ClearSlots();
	Super::NativeDestruct();
}

void UAbilityBarWidget::ClearSlots()
{
	for (UAbilitySlotWidget* AbilitySlot : SlotWidgets)
	{
		if (IsValid(Slot))
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