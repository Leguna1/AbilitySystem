#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryWidget.generated.h"

class UEquipmentDataAsset;
class UInventoryComponent;
class UInventorySlotWidget;
class UPanelWidget;

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
 *   4. Show it: Create Widget > Add to Viewport. With Auto Initialize on, it finds the owning player's
 *      Inventory Component itself; otherwise call Initialize Inventory.
 *      For dragging, the cursor must be visible: Set Show Mouse Cursor on the player controller
 *      and Set Input Mode Game And UI (back to Game Only when closing).
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void InitializeInventory(UInventoryComponent* InInventory);

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

private:
	UFUNCTION()
	void HandleInventorySlotChanged(int32 SlotIndex);

	void ClearCreatedSlots();
	void Unbind();

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;

	/** Index = bag slot index. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventorySlotWidget>> BagSlotWidgets;

	/** Index = equipment slot index. Null where the inventory has a slot no widget shows. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInventorySlotWidget>> EquipmentSlotWidgets;
};
