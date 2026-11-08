#include "InventoryWidget.h"

#include "AbilitySystemLog.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterMenuTabButton.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "CurrencyDataAsset.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "EquipmentDataAsset.h"
#include "GameFramework/Pawn.h"
#include "InventoryComponent.h"
#include "InventorySlotWidget.h"

#define LOCTEXT_NAMESPACE "InventoryWidget"

UInventoryWidget::UInventoryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FInventoryFilterTab AllTab;
	AllTab.Label = LOCTEXT("AllTab", "All");
	FilterTabs.Add(AllTab);

	FInventoryFilterTab EquipmentTab;
	EquipmentTab.Label = LOCTEXT("EquipmentTab", "Equipment");
	EquipmentTab.Categories.Add(EItemCategory::Equipment);
	FilterTabs.Add(EquipmentTab);

	FInventoryFilterTab ConsumableTab;
	ConsumableTab.Label = LOCTEXT("ConsumableTab", "Consumable");
	ConsumableTab.Categories.Add(EItemCategory::Consumable);
	FilterTabs.Add(ConsumableTab);
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	BuildFilterTabs();

	if (bAutoInitialize && !IsValid(Inventory))
	{
		if (const APawn* Pawn = GetOwningPlayerPawn())
		{
			InitializeInventory(Pawn->FindComponentByClass<UInventoryComponent>());
		}
	}
}

void UInventoryWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UInventoryWidget::InitializeInventory(UInventoryComponent* InInventory)
{
	Unbind();
	Inventory = InInventory;

	if (IsValid(Inventory))
	{
		Inventory->OnInventorySlotChanged.AddDynamic(this, &UInventoryWidget::HandleInventorySlotChanged);
		Inventory->OnCurrencyChanged.AddDynamic(this, &UInventoryWidget::HandleCurrencyChanged);
	}

	RebuildSlots();
	RefreshCurrency(0);
}

/* -------------------- Currency -------------------- */

void UInventoryWidget::HandleCurrencyChanged(UCurrencyDataAsset* Currency, const int32 NewAmount, const int32 Delta)
{
	if (Currency == DisplayedCurrency)
	{
		RefreshCurrency(Delta);
	}
}

void UInventoryWidget::RefreshCurrency(const int32 Delta)
{
	if (!IsValid(DisplayedCurrency))
	{
		return;
	}

	const int32 Amount = IsValid(Inventory) ? Inventory->GetCurrency(DisplayedCurrency) : 0;

	if (IsValid(CurrencyText))
	{
		// AsNumber adds thousands separators for the player's language (12,345).
		CurrencyText->SetText(FText::AsNumber(Amount));
	}

	OnDisplayedCurrencyChanged(Amount, Delta);
}

void UInventoryWidget::RebuildSlots()
{
	ClearCreatedSlots();

	if (!IsValid(Inventory))
	{
		return;
	}

	/* ---- Bag ---- */

	if (IsValid(BagContainer) && SlotWidgetClass)
	{
		UUniformGridPanel* Grid = Cast<UUniformGridPanel>(BagContainer);
		const int32 RowLength = FMath::Max(Columns, 1);

		for (int32 SlotIndex = 0; SlotIndex < Inventory->GetNumSlots(); ++SlotIndex)
		{
			UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, SlotWidgetClass);
			SlotWidget->InitializeBagSlot(Inventory, SlotIndex);

			if (Grid)
			{
				Grid->AddChildToUniformGrid(SlotWidget, SlotIndex / RowLength, SlotIndex % RowLength);
			}
			else
			{
				BagContainer->AddChild(SlotWidget);
			}

			BagSlotWidgets.Add(SlotWidget);
			BagSlotVisibility.Add(SlotWidget->GetVisibility());
		}

		BagSlotShown.Init(true, BagSlotWidgets.Num());
		ApplyFilter();
	}
	else
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("%s: needs a Bag Container and a Slot Widget Class to show the bag."), *GetName());
	}

	/* ---- Equipment ---- */

	EquipmentSlotWidgets.Init(nullptr, Inventory->GetNumEquipmentSlots());

	// Hand-placed slots first: a paper doll decides where each one sits.
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([this](UWidget* Widget)
		{
			UInventorySlotWidget* SlotWidget = Cast<UInventorySlotWidget>(Widget);

			if (!SlotWidget || !SlotWidget->GetEquipmentSlotTag().IsValid())
			{
				return;
			}

			const int32 EquipmentSlotIndex = Inventory->FindEquipmentSlotIndex(SlotWidget->GetEquipmentSlotTag());

			if (EquipmentSlotIndex == INDEX_NONE)
			{
				UE_LOG(LogAbilitySystem, Warning, TEXT("%s: slot widget %s is for %s, which the inventory has no equipment slot for."),
					*GetName(), *SlotWidget->GetName(), *SlotWidget->GetEquipmentSlotTag().ToString());
				return;
			}

			SlotWidget->InitializeEquipmentSlot(Inventory, EquipmentSlotIndex);
			EquipmentSlotWidgets[EquipmentSlotIndex] = SlotWidget;
		});
	}

	const TSubclassOf<UInventorySlotWidget> EquipmentClass = EquipmentSlotWidgetClass ? EquipmentSlotWidgetClass : SlotWidgetClass;

	if (IsValid(EquipmentContainer) && EquipmentClass)
	{
		for (int32 EquipmentSlotIndex = 0; EquipmentSlotIndex < EquipmentSlotWidgets.Num(); ++EquipmentSlotIndex)
		{
			if (EquipmentSlotWidgets[EquipmentSlotIndex])
			{
				continue;
			}

			UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, EquipmentClass);
			SlotWidget->InitializeEquipmentSlot(Inventory, EquipmentSlotIndex);
			EquipmentContainer->AddChild(SlotWidget);
			EquipmentSlotWidgets[EquipmentSlotIndex] = SlotWidget;
		}
	}
}

void UInventoryWidget::HandleInventorySlotChanged(const int32 SlotIndex)
{
	if (BagSlotWidgets.IsValidIndex(SlotIndex))
	{
		BagSlotWidgets[SlotIndex]->RefreshSlot();

		// A potion moved into a slot the Equipment tab showed: it leaves the view (and the rest close up).
		if (PassesFilter(SlotIndex) != BagSlotShown[SlotIndex])
		{
			ApplyFilter();
		}
		return;
	}

	// Opened before the inventory's BeginPlay sized the bag: build now that there is one.
	if (IsValid(Inventory) && (BagSlotWidgets.Num() != Inventory->GetNumSlots() || EquipmentSlotWidgets.Num() != Inventory->GetNumEquipmentSlots()))
	{
		RebuildSlots();
		return;
	}

	for (UInventorySlotWidget* SlotWidget : BagSlotWidgets)
	{
		SlotWidget->RefreshSlot();
	}

	ApplyFilter();
}

/* -------------------- Filter -------------------- */

void UInventoryWidget::BuildFilterTabs()
{
	for (UCharacterMenuTabButton* TabButton : FilterTabButtons)
	{
		if (IsValid(TabButton))
		{
			TabButton->RemoveFromParent();
		}
	}

	FilterTabButtons.Reset();

	if (!IsValid(FilterTabContainer) || !FilterTabButtonClass)
	{
		return;
	}

	for (int32 TabIndex = 0; TabIndex < FilterTabs.Num(); ++TabIndex)
	{
		UCharacterMenuTabButton* TabButton = CreateWidget<UCharacterMenuTabButton>(this, FilterTabButtonClass);
		TabButton->SetupTab(TabIndex, FilterTabs[TabIndex].Label);
		TabButton->SetKeyText(FText::GetEmpty());
		TabButton->SetSelected(TabIndex == FilterTabIndex);
		TabButton->OnTabClicked.AddUniqueDynamic(this, &UInventoryWidget::HandleFilterTabClicked);
		FilterTabContainer->AddChild(TabButton);
		FilterTabButtons.Add(TabButton);
	}
}

void UInventoryWidget::HandleFilterTabClicked(const int32 ClickedTabIndex)
{
	SetFilterTab(ClickedTabIndex);
}

void UInventoryWidget::SetFilterTab(const int32 TabIndex)
{
	if (TabIndex == FilterTabIndex)
	{
		return;
	}

	FilterTabIndex = TabIndex;

	for (UCharacterMenuTabButton* TabButton : FilterTabButtons)
	{
		if (IsValid(TabButton))
		{
			TabButton->SetSelected(TabButton->GetTabIndex() == FilterTabIndex);
		}
	}

	ApplyFilter();
	OnFilterTabChanged(FilterTabIndex);
}

bool UInventoryWidget::PassesFilter(const int32 SlotIndex) const
{
	if (!IsValid(Inventory) || !FilterTabs.IsValidIndex(FilterTabIndex))
	{
		return true;
	}

	const FInventoryFilterTab& Tab = FilterTabs[FilterTabIndex];
	const FInventorySlot InventorySlot = Inventory->GetSlot(SlotIndex);

	if (InventorySlot.IsEmpty())
	{
		return Tab.Categories.IsEmpty() || bShowEmptySlotsWhenFiltered;
	}

	return Tab.Shows(InventorySlot.Item);
}

void UInventoryWidget::ApplyFilter()
{
	UUniformGridPanel* Grid = Cast<UUniformGridPanel>(BagContainer);
	const int32 RowLength = FMath::Max(Columns, 1);
	int32 DisplayIndex = 0;

	for (int32 SlotIndex = 0; SlotIndex < BagSlotWidgets.Num(); ++SlotIndex)
	{
		UInventorySlotWidget* SlotWidget = BagSlotWidgets[SlotIndex];
		const bool bShown = PassesFilter(SlotIndex);

		BagSlotShown[SlotIndex] = bShown;
		SlotWidget->SetVisibility(bShown ? BagSlotVisibility[SlotIndex] : ESlateVisibility::Collapsed);

		if (!bShown)
		{
			continue;
		}

		// A grid places children by row and column, so shown slots are moved up to close the gaps.
		// Other panels (a Wrap Box) flow around collapsed children by themselves.
		if (Grid)
		{
			if (UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(SlotWidget->Slot))
			{
				GridSlot->SetRow(DisplayIndex / RowLength);
				GridSlot->SetColumn(DisplayIndex % RowLength);
			}
		}

		++DisplayIndex;
	}
}

void UInventoryWidget::ClearCreatedSlots()
{
	// Only remove what this widget created; hand-placed equipment slots belong to the designer layout.
	if (IsValid(BagContainer))
	{
		for (UInventorySlotWidget* SlotWidget : BagSlotWidgets)
		{
			SlotWidget->RemoveFromParent();
		}
	}

	if (IsValid(EquipmentContainer))
	{
		for (UInventorySlotWidget* SlotWidget : EquipmentSlotWidgets)
		{
			// Created slots have no tag of their own; tagged ones were placed by hand, even inside this panel.
			if (SlotWidget && !SlotWidget->GetEquipmentSlotTag().IsValid() && SlotWidget->GetParent() == EquipmentContainer)
			{
				SlotWidget->RemoveFromParent();
			}
		}
	}

	BagSlotWidgets.Reset();
	BagSlotShown.Reset();
	BagSlotVisibility.Reset();
	EquipmentSlotWidgets.Reset();
}

void UInventoryWidget::Unbind()
{
	if (IsValid(Inventory))
	{
		Inventory->OnInventorySlotChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventorySlotChanged);
		Inventory->OnCurrencyChanged.RemoveDynamic(this, &UInventoryWidget::HandleCurrencyChanged);
	}
}

#undef LOCTEXT_NAMESPACE
