#include "AbilityBarWidget.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "AbilitySlotWidget.h"
#include "ActionBarComponent.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBoxSlot.h"

void UAbilityBarWidget::InitializeBar(UAbilityComponent* InAbilityComponent)
{
	Unbind();

	AbilityComponent = InAbilityComponent;
	ActionBar = IsValid(AbilityComponent) ? AbilityComponent->GetOwner()->FindComponentByClass<UActionBarComponent>() : nullptr;

	if (IsValid(ActionBar))
	{
		ActionBar->OnActionBarChanged.AddUniqueDynamic(this, &UAbilityBarWidget::HandleActionBarChanged);
	}
	else if (IsValid(AbilityComponent))
	{
		AbilityComponent->GrantedAbilitiesChangedEvent.AddUniqueDynamic(this, &UAbilityBarWidget::HandleGrantedAbilitiesChanged);
	}

	RebuildSlots();
}

void UAbilityBarWidget::RebuildSlots()
{
	ClearSlots();

	if (!IsValid(AbilityComponent) || !IsValid(SlotContainer) || !SlotWidgetClass)
	{
		return;
	}

	if (IsValid(ActionBar))
	{
		for (int32 SlotIndex = 0; SlotIndex < ActionBar->GetNumSlots(); ++SlotIndex)
		{
			UAbilitySlotWidget* AbilitySlot = CreateWidget<UAbilitySlotWidget>(this, SlotWidgetClass);

			if (IsValid(AbilitySlot))
			{
				AbilitySlot->SetActionBarSlot(ActionBar, SlotIndex);
				AddSlotWidget(AbilitySlot);
				AbilitySlot->InitializeSlot(AbilityComponent, ActionBar->GetSlotAbility(SlotIndex));
			}
		}

		return;
	}

	// No action bar: one read-only slot per granted ability, as before.
	for (const TSubclassOf<UAbility>& AbilityClass : AbilityComponent->GetGrantedAbilityClasses())
	{
		UAbilitySlotWidget* AbilitySlot = AbilityClass ? CreateWidget<UAbilitySlotWidget>(this, SlotWidgetClass) : nullptr;

		if (IsValid(AbilitySlot))
		{
			AddSlotWidget(AbilitySlot);
			AbilitySlot->InitializeSlot(AbilityComponent, AbilityClass);
		}
	}
}

void UAbilityBarWidget::AddSlotWidget(UAbilitySlotWidget* AbilitySlot)
{
	const bool bFirst = SlotWidgets.IsEmpty();
	UPanelSlot* PanelSlot = SlotContainer->AddChild(AbilitySlot);
	SlotWidgets.Add(AbilitySlot);

	// The gap goes before every slot but the first.
	const float Gap = bFirst ? 0.0f : SlotSpacing;

	if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(PanelSlot))
	{
		HorizontalSlot->SetPadding(FMargin(Gap, 0.0f, 0.0f, 0.0f));
	}
	else if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(PanelSlot))
	{
		VerticalSlot->SetPadding(FMargin(0.0f, Gap, 0.0f, 0.0f));
	}
}

void UAbilityBarWidget::HandleActionBarChanged(const int32 SlotIndex)
{
	if (SlotIndex == INDEX_NONE || SlotWidgets.Num() != ActionBar->GetNumSlots())
	{
		for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
		{
			RefreshSlot(Index);
		}

		return;
	}

	RefreshSlot(SlotIndex);
}

void UAbilityBarWidget::RefreshSlot(const int32 SlotIndex)
{
	if (SlotWidgets.IsValidIndex(SlotIndex) && IsValid(SlotWidgets[SlotIndex]) && IsValid(ActionBar))
	{
		SlotWidgets[SlotIndex]->InitializeSlot(AbilityComponent, ActionBar->GetSlotAbility(SlotIndex));
	}
}

void UAbilityBarWidget::HandleGrantedAbilitiesChanged()
{
	RebuildSlots();
}

void UAbilityBarWidget::NativeDestruct()
{
	Unbind();
	ClearSlots();
	Super::NativeDestruct();
}

void UAbilityBarWidget::Unbind()
{
	if (IsValid(ActionBar))
	{
		ActionBar->OnActionBarChanged.RemoveDynamic(this, &UAbilityBarWidget::HandleActionBarChanged);
	}

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->GrantedAbilitiesChangedEvent.RemoveDynamic(this, &UAbilityBarWidget::HandleGrantedAbilitiesChanged);
	}
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
