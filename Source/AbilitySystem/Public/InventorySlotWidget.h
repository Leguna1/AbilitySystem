#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "InventoryDragDropOperation.h"
#include "InventorySlotWidget.generated.h"

class UImage;
class UInventoryComponent;
class UItemDataAsset;
class UTextBlock;
class UTexture2D;
class UWidget;

/**
 * One inventory slot: a bag slot or an equipment slot. Shows the item and edits the inventory:
 *   - drag a bag item onto another bag slot to move, swap or merge stacks,
 *   - drag equipment onto its equipment slot (or right-click it) to wear it,
 *   - drag worn equipment onto a bag slot (or right-click it) to take it off,
 *   - right-click a consumable to use it. Its cooldown shows on every slot holding the same cooldown group.
 *
 * Blueprint setup (Widget Blueprint with this as parent class, e.g. WBP_InventorySlot):
 *   - Root: a Size Box (64x64) holding an Overlay, so the slot has a fixed size and something to hit-test.
 *   - Optional Image named "IconImage": the item icon, or the equipment slot's Empty Icon while empty.
 *   - Optional Text Block named "CountText": the stack count, hidden at 1 or less.
 *   - Optional Image named "CooldownImage" over the icon: shown only while the item's cooldown runs.
 *     Give it a material with a scalar "Cooldown" parameter (1 = just started, 0 = ready) for a radial sweep.
 *   - Optional Text Block named "CooldownText": whole seconds left, shown only while on cooldown.
 *   - Optional: a border or frame image, and a highlight you show in On Drag Hover Changed.
 *   - Override Build Tooltip Widget to return your item tooltip (Get Item gives the data), and
 *     Build Drag Visual for a custom drag image (default: the icon at Drag Icon Size).
 * The same Blueprint can serve both kinds; make a second one if equipment slots should look different.
 *
 * Equipment slots placed by hand in the Inventory Widget (a paper doll): set Equipment Slot Tag on
 * the instance in the designer, e.g. Equipment.Slot.Feet. The Inventory Widget finds and initializes them.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Makes this bag slot SlotIndex of Inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void InitializeBagSlot(UInventoryComponent* InInventory, int32 InSlotIndex);

	/** Makes this equipment slot EquipmentSlotIndex of Inventory. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void InitializeEquipmentSlot(UInventoryComponent* InInventory, int32 InEquipmentSlotIndex);

	/** Re-reads the item from the inventory and updates the visuals. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|UI")
	void RefreshSlot();

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	EInventorySlotKind GetSlotKind() const { return SlotKind; }

	/** Bag slot index or equipment slot index, by Slot Kind. */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	UInventoryComponent* GetInventory() const { return Inventory; }

	/** The item shown, or null when empty. */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	UItemDataAsset* GetItem() const { return Item; }

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	int32 GetCount() const { return Count; }

	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	bool IsEmpty() const { return Item == nullptr; }

	/** For hand-placed equipment slots: which slot this is. Read by the Inventory Widget. */
	FGameplayTag GetEquipmentSlotTag() const { return EquipmentSlotTag; }

	/** True while the shown item's cooldown group is waiting. */
	UFUNCTION(BlueprintPure, Category = "Inventory|UI")
	bool IsOnCooldown() const { return bOnCooldown; }

protected:
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Would dropping Operation here do anything? Drives the hover highlight. */
	bool CanAcceptDrop(const UInventoryDragDropOperation* Operation) const;

	/**
	 * Placed by hand as an equipment slot in an Inventory Widget: the equipment slot this is
	 * (e.g. Equipment.Slot.Feet). Leave empty on bag slots.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI", meta = (Categories = "Equipment.Slot"))
	FGameplayTag EquipmentSlotTag;

	/** Right-click a bag item to wear it, or a worn item to take it off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	bool bRightClickEquips = true;

	/** Right-click a consumable (a potion) to use it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	bool bRightClickUses = true;

	/** Scalar parameter on Cooldown Image's material: 1 = cooldown just started, 0 = ready. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI", AdvancedDisplay)
	FName CooldownParameter = TEXT("Cooldown");

	/** How often the cooldown sweep updates, in seconds. Only runs while on cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI", AdvancedDisplay, meta = (ClampMin = "0.01", Units = "s"))
	float CooldownPollInterval = 0.05f;

	/** Size of the default drag image (the icon) when Build Drag Visual returns nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	FVector2D DragIconSize = FVector2D(56.0f, 56.0f);

	/** Icon tint while showing an equipment slot's Empty Icon, so it reads as a silhouette. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory|UI")
	FLinearColor EmptyIconTint = FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);

	/* -------------------- Kit visuals (optional) -------------------- */

	/** The item icon (or the empty slot's silhouette). Name it IconImage. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|UI|Widgets")
	TObjectPtr<UImage> IconImage;

	/** Stack count, hidden at 1 or less. Name it CountText. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|UI|Widgets")
	TObjectPtr<UTextBlock> CountText;

	/** Cooldown overlay over the icon, shown only while on cooldown. Name it CooldownImage. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|UI|Widgets")
	TObjectPtr<UImage> CooldownImage;

	/** Whole seconds left on the cooldown. Name it CooldownText. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Inventory|UI|Widgets")
	TObjectPtr<UTextBlock> CooldownText;

	/* -------------------- Blueprint events -------------------- */

	/** The item or count changed (empty included). The kit visuals are already updated. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|UI")
	void OnSlotChanged(UItemDataAsset* NewItem, int32 NewCount);

	/** Something is dragged over this slot (bHovered), and whether letting go here would work. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|UI")
	void OnDragHoverChanged(bool bHovered, bool bCanDrop);

	/** The widget that follows the cursor while dragging. Null = the icon. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|UI")
	UWidget* BuildDragVisual();

	/** Return the tooltip widget on hover, or null for none. Only asked while the slot holds an item. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|UI")
	UWidget* BuildTooltipWidget();

	/** The item's cooldown started (true) or finished, or the slot changed to an item with a different state. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|UI")
	void OnCooldownStateChanged(bool bOnCooldownNow);

	/** Each poll while on cooldown. Progress: 1 = just started, 0 = ready. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory|UI")
	void OnCooldownUpdated(float Remaining, float Progress);

private:
	UFUNCTION()
	UWidget* GetTooltipWidget();

	void RefreshKitVisuals();

	/** Points at a (possibly different) inventory: moves the cooldown listener over. */
	void SetInventory(UInventoryComponent* InInventory);

	UFUNCTION()
	void HandleItemCooldownStarted(FGameplayTag CooldownTag, float Duration);

	/** Starts or stops the poll to match the shown item's cooldown. */
	void RefreshCooldown();
	void TickCooldown();
	void StopCooldownPoll();
	void SetCooldownState(bool bNewOnCooldown);
	void ApplyCooldownVisuals(float Remaining, float Progress);

	FTimerHandle CooldownTimerHandle;
	bool bOnCooldown = false;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TObjectPtr<UItemDataAsset> Item;

	EInventorySlotKind SlotKind = EInventorySlotKind::Bag;
	int32 SlotIndex = INDEX_NONE;
	int32 Count = 0;
};
