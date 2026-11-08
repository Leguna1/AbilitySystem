#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemDataAsset.h"
#include "InventoryWidget.generated.h"

class UCharacterMenuTabButton;
class UEquipmentDataAsset;
class UInventoryComponent;
class UInventorySlotWidget;
class UPanelWidget;

/** One tab above the bag: shows only items of its categories. */
USTRUCT(BlueprintType)
struct FInventoryFilterTab
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	FText Label;

	/** Items of these categories are shown. Empty = every item (an "All" tab). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	TArray<EItemCategory> Categories;

	bool Shows(const UItemDataAsset* Item) const { return Categories.IsEmpty() || (Item && Categories.Contains(Item->GetCategory())); }
};

/**
 * The inventory window: a grid of bag slots and the equipment slots. Shows an Inventory Component;
 * all editing (drag and drop, right-click) happens in the slots.
 *
 * Blueprint setup (Widget Blueprint with this as parent class, e.g. WBP_Inventory):
 *   1. A Uniform Grid Panel named "BagContainer". One slot per bag slot is created in it, Columns wide.
 *      (A Wrap Box or any other panel works too; it is just filled in order.)
 *   2. Set Slot Widget Class to your Inventory Slot Widget Blueprint (WBP_InventorySlot).
 *   3. Equipment slots, either way (or both):
 *      a) Paper doll: place WBP_InventorySlot instances by hand around a character portrait and set
 *         Equipment Slot Tag on each (Equipment.Slot.Head, .Feet...). They're found and set up for you.
 *      b) A panel named "EquipmentContainer" (Vertical Box...): one slot is created per equipment slot
 *         not placed by hand, using Equipment Slot Widget Class (empty = Slot Widget Class).
 *      To show equipment somewhere else instead (around the Character Menu's preview), leave both out of
 *      this widget and place the tagged slots there; the Character Menu sets them up.
 *   4. Optional filter tabs: a panel named "FilterTabContainer" (Horizontal Box) and Filter Tab Button Class
 *      (a Character Menu Tab Button Blueprint, e.g. WB_CharacterMenuTab). One button per Filter Tab is made
 *      in it (All / Equipment / Consumable by default). Or call Set Filter Tab from your own buttons.
 *      Optional: a button for Sort Items on the inventory, which packs the bag by category.
 *   5. Show it: Create Widget > Add to Viewport. With Auto Initialize on, it finds the owning player's
 *      Inventory Component itself; otherwise call Initialize Inventory.
 *      For dragging, the cursor must be visible: Set Show Mouse Cursor on the player controller
 *      and Set Input Mode Game And UI (back to Game Only when closing).
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventoryWidget(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void InitializeInventory(UInventoryComponent* InInventory);

	/** Shows only the items of Filter Tabs[TabIndex]. Out of range = show everything. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void SetFilterTab(int32 TabIndex);

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	int32 GetFilterTab() const { return FilterTabIndex; }

	/** Clears and recreates every slot widget. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RebuildSlots();

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	UInventoryComponent* GetInventory() const { return Inventory; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Bag slots are created in here. A Uniform Grid Panel gets them laid out in rows of Columns. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Inventory|UI")
	TObjectPtr<UPanelWidget> BagContainer;

	/** Optional: equipment slots not placed by hand are created in here. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|UI")
	TObjectPtr<UPanelWidget> EquipmentContainer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

	/** For slots created in Equipment Container. Empty = Slot Widget Class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	TSubclassOf<UInventorySlotWidget> EquipmentSlotWidgetClass;

	/** Bag slots per row, when Bag Container is a Uniform Grid Panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI", meta = (ClampMin = "1"))
	int32 Columns = 6;

	/** On construct, show the owning player's pawn's Inventory Component without being told to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	bool bAutoInitialize = true;

	/** Tabs above the bag, in order. The first is open when the widget opens. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI|Filter", meta = (TitleProperty = "Label"))
	TArray<FInventoryFilterTab> FilterTabs;

	/** Optional: one Filter Tab Button Class button per filter tab is created in here. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Inventory|UI|Filter")
	TObjectPtr<UPanelWidget> FilterTabContainer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI|Filter")
	TSubclassOf<UCharacterMenuTabButton> FilterTabButtonClass;

	/**
	 * Show empty bag slots in filtered tabs too, so there is somewhere to drop an unequipped item.
	 * Off = a filtered tab lists only its items.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI|Filter")
	bool bShowEmptySlotsWhenFiltered = true;

	/** The open filter tab changed. Hook a sound or an animation here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|UI")
	void OnFilterTabChanged(int32 NewTabIndex);

private:
	UFUNCTION()
	void HandleInventorySlotChanged(int32 SlotIndex);

	UFUNCTION()
	void HandleFilterTabClicked(int32 ClickedTabIndex);

	void BuildFilterTabs();

	/** Does bag slot SlotIndex show in the open tab? */
	bool PassesFilter(int32 SlotIndex) const;

	/**
	 * Shows the slots that pass the filter, packed from the first cell, and collapses the rest. Widgets
	 * are kept, not removed: removing one would unbind it from the inventory.
	 */
	void ApplyFilter();

	void ClearCreatedSlots();
	void Unbind();

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;

	/** Index = bag slot index. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventorySlotWidget>> BagSlotWidgets;

	/** Index = bag slot index: whether it currently passes the filter. */
	TArray<bool> BagSlotShown;

	/** The visibility a bag slot widget had when created, restored when the filter shows it again. */
	TArray<ESlateVisibility> BagSlotVisibility;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCharacterMenuTabButton>> FilterTabButtons;

	int32 FilterTabIndex = 0;

	/** Index = equipment slot index. Null where the inventory has a slot no widget shows. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventorySlotWidget>> EquipmentSlotWidgets;
};
