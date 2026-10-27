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
	ActionBar = bUseActionBar && IsValid(AbilityComponent)
		? AbilityComponent->GetOwner()->FindComponentByClass<UActionBarComponent>()
		: nullptr;

	// An action bar without slots (left over from an older setup) shows nothing: use the abilities instead.
	if (IsValid(ActionBar) && ActionBar->GetNumSlots() == 0)
	{
		ActionBar = nullptr;
	}

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

void UAbilityBarWidget::GetShownActionBarSlots(TArray<int32>& OutSlots) const
{
	OutSlots.Reset();

	if (!IsValid(ActionBar))
	{
		return;
	}

	if (SlotIndices.IsEmpty())
	{
		for (int32 Index = 0; Index < ActionBar->GetNumSlots(); ++Index)
		{
			OutSlots.Add(Index);
		}

		return;
	}

	for (const int32 Index : SlotIndices)
	{
		if (Index >= 0 && Index < ActionBar->GetNumSlots())
		{
			OutSlots.Add(Index);
		}
	}
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
		TArray<int32> BarSlots;
		GetShownActionBarSlots(BarSlots);

		for (const int32 BarIndex : BarSlots)
		{
			UAbilitySlotWidget* AbilitySlot = CreateWidget<UAbilitySlotWidget>(this, SlotWidgetClass);

			if (IsValid(AbilitySlot))
			{
				AbilitySlot->SetActionBarSlot(ActionBar, BarIndex);
				AddSlotWidget(AbilitySlot);
				ShownSlotIndices.Add(BarIndex);
				AbilitySlot->InitializeSlot(AbilityComponent, ActionBar->GetSlotAbility(BarIndex));
			}
		}

		return;
	}

	// No action bar: one read-only slot per granted ability with an icon (skips hit reactions and other hidden ones).
	int32 Shown = 0;

	for (const TSubclassOf<UAbility>& AbilityClass : AbilityComponent->GetGrantedAbilityClasses())
	{
		if (MaxGrantedSlots > 0 && Shown >= MaxGrantedSlots)
		{
			break;
		}

		const UAbility* Defaults = AbilityClass ? AbilityComponent->GetAbilityDefaults(AbilityClass) : nullptr;

		if (!IsValid(Defaults) || !IsValid(Defaults->GetIcon()))
		{
			continue;
		}

		UAbilitySlotWidget* AbilitySlot = CreateWidget<UAbilitySlotWidget>(this, SlotWidgetClass);

		if (IsValid(AbilitySlot))
		{
			AddSlotWidget(AbilitySlot);
			AbilitySlot->InitializeSlot(AbilityComponent, AbilityClass);
			++Shown;
		}
	}

	for (; Shown < MinGrantedSlots; ++Shown)
	{
		if (UAbilitySlotWidget* EmptySlot = CreateWidget<UAbilitySlotWidget>(this, SlotWidgetClass))
		{
			AddSlotWidget(EmptySlot);
			EmptySlot->InitializeSlot(AbilityComponent, nullptr);
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
	// SlotIndex is an action bar slot; this bar may show only some of them.
	const int32 WidgetIndex = SlotIndex == INDEX_NONE ? INDEX_NONE : ShownSlotIndices.IndexOfByKey(SlotIndex);

	if (SlotIndex == INDEX_NONE)
	{
		for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
		{
			RefreshSlot(Index);
		}

		return;
	}

	RefreshSlot(WidgetIndex);
}

void UAbilityBarWidget::RefreshSlot(const int32 WidgetIndex)
{
	if (SlotWidgets.IsValidIndex(WidgetIndex) &&
		ShownSlotIndices.IsValidIndex(WidgetIndex) &&
		IsValid(SlotWidgets[WidgetIndex]) &&
		IsValid(ActionBar))
	{
		SlotWidgets[WidgetIndex]->InitializeSlot(AbilityComponent, ActionBar->GetSlotAbility(ShownSlotIndices[WidgetIndex]));
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
	ShownSlotIndices.Reset();

	if (IsValid(SlotContainer))
	{
		SlotContainer->ClearChildren();
	}
}
