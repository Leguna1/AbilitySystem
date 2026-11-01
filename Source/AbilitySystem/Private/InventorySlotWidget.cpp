#include "InventorySlotWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EquipmentDataAsset.h"
#include "InputCoreTypes.h"
#include "InventoryComponent.h"
#include "ItemDataAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

void UInventorySlotWidget::InitializeBagSlot(UInventoryComponent* InInventory, const int32 InSlotIndex)
{
	SetInventory(InInventory);
	SlotKind = EInventorySlotKind::Bag;
	SlotIndex = InSlotIndex;
	RefreshSlot();
}

void UInventorySlotWidget::InitializeEquipmentSlot(UInventoryComponent* InInventory, const int32 InEquipmentSlotIndex)
{
	SetInventory(InInventory);
	SlotKind = EInventorySlotKind::Equipment;
	SlotIndex = InEquipmentSlotIndex;
	RefreshSlot();
}

void UInventorySlotWidget::RefreshSlot()
{
	Item = nullptr;
	Count = 0;

	if (IsValid(Inventory))
	{
		if (SlotKind == EInventorySlotKind::Bag)
		{
			const FInventorySlot InventorySlot = Inventory->GetSlot(SlotIndex);

			if (!InventorySlot.IsEmpty())
			{
				Item = InventorySlot.Item;
				Count = InventorySlot.Count;
			}
		}
		else if (UEquipmentDataAsset* Worn = Inventory->GetEquippedItem(SlotIndex))
		{
			Item = Worn;
			Count = 1;
		}
	}

	ToolTipWidgetDelegate.Unbind();

	if (!IsEmpty())
	{
		// Built on hover, not up front.
		ToolTipWidgetDelegate.BindDynamic(this, &UInventorySlotWidget::GetTooltipWidget);
	}

	RefreshKitVisuals();
	OnSlotChanged(Item, Count);

	// A potion moved here may be mid-cooldown, or the slot emptied while one was running.
	RefreshCooldown();
}

void UInventorySlotWidget::NativeDestruct()
{
	SetInventory(nullptr);
	StopCooldownPoll();
	Super::NativeDestruct();
}

void UInventorySlotWidget::SetInventory(UInventoryComponent* InInventory)
{
	if (Inventory == InInventory)
	{
		return;
	}

	if (IsValid(Inventory))
	{
		Inventory->OnItemCooldownStarted.RemoveDynamic(this, &UInventorySlotWidget::HandleItemCooldownStarted);
	}

	Inventory = InInventory;

	if (IsValid(Inventory))
	{
		Inventory->OnItemCooldownStarted.AddUniqueDynamic(this, &UInventorySlotWidget::HandleItemCooldownStarted);
	}
}

/* -------------------- Editing: right-click, drag and drop -------------------- */

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsValid(Inventory) || IsEmpty())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FKey Button = InMouseEvent.GetEffectingButton();

	if (Button == EKeys::RightMouseButton && bRightClickUses && SlotKind == EInventorySlotKind::Bag
		&& Item->GetCategory() == EItemCategory::Consumable)
	{
		// Refused (on cooldown, full health) does nothing; the sweep already says why.
		Inventory->UseItem(SlotIndex);
		return FReply::Handled();
	}

	if (Button == EKeys::RightMouseButton && bRightClickEquips)
	{
		if (SlotKind == EInventorySlotKind::Equipment)
		{
			Inventory->Unequip(SlotIndex);
		}
		else if (Inventory->CanEquip(Item))
		{
			Inventory->EquipFromSlot(SlotIndex);
		}

		return FReply::Handled();
	}

	if (Button == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (IsEmpty() || !IsValid(Inventory))
	{
		return;
	}

	UInventoryDragDropOperation* Operation = NewObject<UInventoryDragDropOperation>(this);
	Operation->Item = Item;
	Operation->SourceKind = SlotKind;
	Operation->SourceIndex = SlotIndex;
	Operation->SourceInventory = Inventory;
	Operation->Pivot = EDragPivot::CenterCenter;

	UWidget* Visual = BuildDragVisual();

	if (!IsValid(Visual) && IsValid(Item->GetIcon()))
	{
		UImage* DragImage = NewObject<UImage>(this);
		DragImage->SetBrushFromTexture(Item->GetIcon());
		DragImage->SetDesiredSizeOverride(DragIconSize);
		Visual = DragImage;
	}

	Operation->DefaultDragVisual = Visual;
	OutOperation = Operation;
}

bool UInventorySlotWidget::CanAcceptDrop(const UInventoryDragDropOperation* Operation) const
{
	if (!Operation || !IsValid(Inventory) || Operation->SourceInventory.Get() != Inventory)
	{
		return false;
	}

	const bool bSameSlot = Operation->SourceKind == SlotKind && Operation->SourceIndex == SlotIndex;

	if (bSameSlot)
	{
		return false;
	}

	if (SlotKind == EInventorySlotKind::Equipment)
	{
		// Only a bag item that fits this slot; equipment slots never trade with each other.
		return Operation->SourceKind == EInventorySlotKind::Bag && Inventory->CanEquip(Operation->Item, SlotIndex);
	}

	if (Operation->SourceKind == EInventorySlotKind::Bag)
	{
		return true;
	}

	// Worn item onto a bag slot: an empty one, or one holding a piece for the same equipment slot.
	return IsEmpty() || Inventory->CanEquip(Item, Operation->SourceIndex);
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	OnDragHoverChanged(false, false);

	const UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation);

	if (!Operation)
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	if (CanAcceptDrop(Operation))
	{
		if (SlotKind == EInventorySlotKind::Equipment)
		{
			Inventory->EquipFromSlot(Operation->SourceIndex, SlotIndex);
		}
		else if (Operation->SourceKind == EInventorySlotKind::Bag)
		{
			Inventory->MoveItem(Operation->SourceIndex, SlotIndex);
		}
		else
		{
			Inventory->Unequip(Operation->SourceIndex, SlotIndex);
		}
	}

	// Handled even when refused: a drop on a slot never falls through to whatever is behind it.
	return true;
}

void UInventorySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (const UInventoryDragDropOperation* Operation = Cast<UInventoryDragDropOperation>(InOperation))
	{
		OnDragHoverChanged(true, CanAcceptDrop(Operation));
	}
}

void UInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	OnDragHoverChanged(false, false);
}

/* -------------------- Visuals -------------------- */

UWidget* UInventorySlotWidget::GetTooltipWidget()
{
	return BuildTooltipWidget();
}

void UInventorySlotWidget::RefreshKitVisuals()
{
	if (IsValid(IconImage))
	{
		UTexture2D* Texture = IsValid(Item) ? Item->GetIcon() : nullptr;
		bool bSilhouette = false;

		// An empty equipment slot shows what goes there (a faded boot outline).
		if (!Texture && SlotKind == EInventorySlotKind::Equipment && IsValid(Inventory))
		{
			Texture = Inventory->GetEquipmentSlotDefinition(SlotIndex).EmptyIcon;
			bSilhouette = Texture != nullptr;
		}

		if (Texture)
		{
			IconImage->SetBrushFromTexture(Texture);
			IconImage->SetColorAndOpacity(bSilhouette ? EmptyIconTint : FLinearColor::White);
			IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			IconImage->SetVisibility(ESlateVisibility::Hidden);
		}
	}

	if (IsValid(CountText))
	{
		CountText->SetText(FText::AsNumber(Count));
		CountText->SetVisibility(Count > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

/* -------------------- Cooldown -------------------- */

void UInventorySlotWidget::HandleItemCooldownStarted(const FGameplayTag CooldownTag, float Duration)
{
	// Every slot holding something in the same group lights up, not only the one clicked.
	const FGameplayTag MyTag = UInventoryComponent::GetItemCooldownTag(Item);

	if (MyTag.IsValid() && MyTag.MatchesTagExact(CooldownTag))
	{
		RefreshCooldown();
	}
}

void UInventorySlotWidget::RefreshCooldown()
{
	const bool bShouldPoll = IsValid(Inventory) && SlotKind == EInventorySlotKind::Bag && Inventory->IsItemOnCooldown(Item);

	if (!bShouldPoll)
	{
		StopCooldownPoll();
		SetCooldownState(false);
		ApplyCooldownVisuals(0.0f, 0.0f);
		return;
	}

	SetCooldownState(true);
	TickCooldown();

	UWorld* World = GetWorld();

	if (bOnCooldown && World && !World->GetTimerManager().IsTimerActive(CooldownTimerHandle))
	{
		World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UInventorySlotWidget::TickCooldown,
			FMath::Max(CooldownPollInterval, 0.01f), true);
	}
}

void UInventorySlotWidget::TickCooldown()
{
	const float Remaining = IsValid(Inventory) ? Inventory->GetItemCooldownRemaining(Item) : 0.0f;

	if (Remaining <= 0.0f)
	{
		StopCooldownPoll();
		SetCooldownState(false);
		ApplyCooldownVisuals(0.0f, 0.0f);
		return;
	}

	const float Progress = Inventory->GetItemCooldownProgress(Item);
	ApplyCooldownVisuals(Remaining, Progress);
	OnCooldownUpdated(Remaining, Progress);
}

void UInventorySlotWidget::StopCooldownPoll()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}
}

void UInventorySlotWidget::SetCooldownState(const bool bNewOnCooldown)
{
	if (bOnCooldown != bNewOnCooldown)
	{
		bOnCooldown = bNewOnCooldown;
		OnCooldownStateChanged(bOnCooldown);
	}
}

void UInventorySlotWidget::ApplyCooldownVisuals(const float Remaining, const float Progress)
{
	const ESlateVisibility Shown = bOnCooldown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (IsValid(CooldownImage))
	{
		CooldownImage->SetVisibility(Shown);

		// Null when the image has a texture rather than a material: then it is a plain overlay.
		UMaterialInstanceDynamic* Material = bOnCooldown && !CooldownParameter.IsNone() ? CooldownImage->GetDynamicMaterial() : nullptr;

		if (IsValid(Material))
		{
			Material->SetScalarParameterValue(CooldownParameter, Progress);
		}
	}

	if (IsValid(CooldownText))
	{
		CooldownText->SetVisibility(Shown);
		CooldownText->SetText(FText::AsNumber(FMath::CeilToInt(Remaining)));
	}
}
