#include "InventoryWidget.h"

#include "AbilitySystemLog.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/UniformGridPanel.h"
#include "EquipmentDataAsset.h"
#include "GameFramework/Pawn.h"
#include "InventoryComponent.h"
#include "InventorySlotWidget.h"

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();

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
	}

	RebuildSlots();
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
		}
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
	EquipmentSlotWidgets.Reset();
}

void UInventoryWidget::Unbind()
{
	if (IsValid(Inventory))
	{
		Inventory->OnInventorySlotChanged.RemoveDynamic(this, &UInventoryWidget::HandleInventorySlotChanged);
	}
}
