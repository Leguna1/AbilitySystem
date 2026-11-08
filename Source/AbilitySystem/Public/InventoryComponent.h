#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InventoryComponent.generated.h"

class UAbilityComponent;
class UCombatantComponent;
class UConsumableDataAsset;
class UEquipmentDataAsset;
class UItemDataAsset;
class UResourceComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UTexture2D;
class UWeaponManagerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventorySlotChangedSignature, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEquipmentChangedSignature, int32, EquipmentSlotIndex, UEquipmentDataAsset*, PreviousItem, UEquipmentDataAsset*, NewItem);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemUsedSignature, UConsumableDataAsset*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FItemCooldownStartedSignature, FGameplayTag, CooldownTag, float, Duration);

/** One inventory slot: an item and how many. Empty when Item is null. */
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	FInventorySlot() = default;
	FInventorySlot(UItemDataAsset* InItem, const int32 InCount) : Item(InItem), Count(InCount) {}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UItemDataAsset> Item;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	int32 Count = 1;

	bool IsEmpty() const { return Item == nullptr || Count <= 0; }
};

/** One place equipment can be worn. */
USTRUCT(BlueprintType)
struct FEquipmentSlotDefinition
{
	GENERATED_BODY()

	/** Equipment whose Equipment Slot is this tag goes here. One slot per tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment.Slot"))
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FText DisplayName;

	/** Silhouette shown while the slot is empty (a boot outline). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<UTexture2D> EmptyIcon;
};

/** Passed in by the character: equipment for Slot Tag is shown on Mesh Component. */
USTRUCT(BlueprintType)
struct FEquipmentMeshBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment", meta = (Categories = "Equipment.Slot"))
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	TObjectPtr<USkeletalMeshComponent> MeshComponent;
};

/** A character mesh component equipment is shown on, and what it shows with nothing worn. */
USTRUCT()
struct FRegisteredEquipmentMesh
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Component;

	UPROPERTY()
	TObjectPtr<USkeletalMesh> DefaultMesh;
};

/**
 * The character's bag and what it wears.
 *
 * Bag: a fixed number of slots, each holding an item asset and a count (stacks up to the item's Max Stack Size).
 * Equipment: one slot per Equipment Slot Definition. Equipping takes the item out of the bag;
 * unequipping puts it back. While worn, an item's stats go to the Combatant Component and its
 * meshes are set on the character's mesh components registered for its slot. An item fits every slot whose
 * tag is its Equipment Slot or under it (Equipment.Slot.Weapon fits .Weapon.Primary and .Weapon.Secondary).
 * Weapons: Weapon Data Assets are equipment too. The Weapon Manager watches its Inventory Weapon Slots and
 * spawns, holsters and draws the weapon actors; the drawn weapon's slot can't change while an ability runs.
 * Consumables: Use Item spends one and starts its Cooldown Group's cooldown. Cooldowns live on the
 * owner's Ability Component (keyed by tag, like ability cooldowns), so the owner needs one for cooldowns to work.
 *
 * Character Blueprint setup: on Begin Play, call Register Equipment Meshes with one binding per body part,
 * e.g. (Equipment.Slot.Head, Helmet), (Equipment.Slot.Feet, Boots). Whatever mesh the component has
 * then is what it goes back to when the slot is emptied. Pose following (Leader Pose) stays the
 * Blueprint's job; the inventory only swaps the Skeletal Mesh.
 *
 * Widgets only show this and ask it for changes (Inventory Widget, Inventory Slot Widget).
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	/* -------------------- Bag -------------------- */

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetNumSlots() const { return Slots.Num(); }

	/** The slot's contents; an empty slot when out of range. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	FInventorySlot GetSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsSlotEmpty(int32 SlotIndex) const;

	/** Total count of Item across every slot. */
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetItemCount(const UItemDataAsset* Item) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 FindFirstEmptySlot() const;

	/** Tops up existing stacks first, then fills empty slots. Returns how many did NOT fit (0 = all added). */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 AddItem(UItemDataAsset* Item, int32 Count = 1);

	/** Takes up to Count from one slot. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItemAt(int32 SlotIndex, int32 Count = 1);

	/** Takes up to Count of Item from anywhere in the bag, last slots first. Returns how many were removed. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 RemoveItem(const UItemDataAsset* Item, int32 Count = 1);

	/** Dragging one bag slot onto another: same item = stacks merge, otherwise the two swap. */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool MoveItem(int32 FromSlotIndex, int32 ToSlotIndex);

	/**
	 * Tidies the bag: merges partial stacks, then orders items by Category (the enum's order: Equipment
	 * before Consumable), then by Display Name, packed into the first slots. For a "Sort" button.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SortItems();

	/* -------------------- Consumables -------------------- */

	/**
	 * True if the item in SlotIndex is a consumable that can be used now: not on cooldown, owner alive,
	 * and it would do something (a health potion at full health refuses unless it also gives a buff).
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Consumables")
	bool CanUseItem(int32 SlotIndex) const;

	/** Uses one of the consumable in SlotIndex: restores, applies its buff, starts its group's cooldown. */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Consumables")
	bool UseItem(int32 SlotIndex);

	/** Uses one of Item from wherever it is in the bag (for hotkeys: "drink a health potion"). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Consumables")
	bool UseItemOfType(const UConsumableDataAsset* Item);

	/** True while Item's cooldown group is waiting. Always false for items without a cooldown. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Consumables")
	bool IsItemOnCooldown(const UItemDataAsset* Item) const;

	/** Seconds until Item's cooldown group is ready, 0 if ready. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Consumables")
	float GetItemCooldownRemaining(const UItemDataAsset* Item) const;

	/** Remaining fraction of Item's cooldown for a sweep: 1 = just started, 0 = ready. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Consumables")
	float GetItemCooldownProgress(const UItemDataAsset* Item) const;

	/** The tag Item's cooldown is stored under, or none if it has no cooldown. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Consumables")
	static FGameplayTag GetItemCooldownTag(const UItemDataAsset* Item);

	/* -------------------- Equipment -------------------- */

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	int32 GetNumEquipmentSlots() const { return EquipmentSlots.Num(); }

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	FEquipmentSlotDefinition GetEquipmentSlotDefinition(int32 EquipmentSlotIndex) const;

	/** Index of the equipment slot with SlotTag, or -1. */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	int32 FindEquipmentSlotIndex(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	UEquipmentDataAsset* GetEquippedItem(int32 EquipmentSlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	UEquipmentDataAsset* GetEquippedItemInSlot(FGameplayTag SlotTag) const;

	/**
	 * True if Item is equipment that fits EquipmentSlotIndex and that slot can change now.
	 * -1 = the slot it would go to (an empty fitting one first).
	 */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	bool CanEquip(const UItemDataAsset* Item, int32 EquipmentSlotIndex = -1) const;

	/** False while the slot holds the drawn weapon and an ability is running (the weapon can't be swapped then). */
	UFUNCTION(BlueprintPure, Category = "Inventory|Equipment")
	bool CanChangeEquipmentSlot(int32 EquipmentSlotIndex) const;

	/**
	 * Wears the item in bag slot SlotIndex. Whatever was worn there goes back into that bag slot.
	 * EquipmentSlotIndex -1 = the slot matching the item's Equipment Slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	bool EquipFromSlot(int32 SlotIndex, int32 EquipmentSlotIndex = -1);

	/**
	 * Takes the item off into bag slot SlotIndex (-1 = the first empty one). If that slot holds equipment
	 * for the same equipment slot, the two swap; any other occupied slot refuses.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	bool Unequip(int32 EquipmentSlotIndex, int32 SlotIndex = -1);

	/**
	 * For each binding, equipment for its Slot Tag is shown on its Mesh Component (Equipment.Slot.Head -> Helmet).
	 * Each component's current mesh becomes the one shown while nothing is worn there. Call on Begin Play.
	 * Parts that aren't inventory slots can be registered too (e.g. Equipment.Slot.Hair), so items can hide them.
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Equipment")
	void RegisterEquipmentMeshes(const TArray<FEquipmentMeshBinding>& Bindings);

	/* -------------------- Events -------------------- */

	/** A bag slot changed. -1 = several or all of them. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventorySlotChangedSignature OnInventorySlotChanged;

	/** Something was put on or taken off (New Item null = slot emptied). */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Equipment")
	FEquipmentChangedSignature OnEquipmentChanged;

	/** A consumable was used (already spent from the bag). Hook drink sounds and effects here. */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Consumables")
	FItemUsedSignature OnItemUsed;

	/**
	 * A cooldown started on the owner. Item cooldowns share the ability cooldown store, so this fires for
	 * abilities too; compare Cooldown Tag with Get Item Cooldown Tag.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Inventory|Consumables")
	FItemCooldownStartedSignature OnItemCooldownStarted;

	// Public to match UActorComponent, so the override doesn't narrow the base class's visibility.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:

	/** How many bag slots. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	int32 NumSlots = 24;

	/** Put in the bag at start, in this order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TArray<FInventorySlot> StartingItems;

	/** Where equipment can be worn, in the order the equipment panel shows them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Equipment", meta = (TitleProperty = "SlotTag"))
	TArray<FEquipmentSlotDefinition> EquipmentSlots;

	/** Worn at start, each in its own Equipment Slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Equipment")
	TArray<TObjectPtr<UEquipmentDataAsset>> StartingEquipment;

private:
	/** Puts Item (or nothing) in an equipment slot: stats, meshes, event. Returns what was there. */
	UEquipmentDataAsset* SetEquipped(int32 EquipmentSlotIndex, UEquipmentDataAsset* Item);

	void ApplyStats(const UEquipmentDataAsset* Item) const;
	void RemoveStats(const UEquipmentDataAsset* Item) const;

	/** Sets every registered part: its default mesh, then each worn item in slot order. */
	void RefreshEquipmentMeshes();

	bool IsValidSlotIndex(int32 SlotIndex) const { return Slots.IsValidIndex(SlotIndex); }

	/** Item's Equipment Slot is the slot's tag or a parent of it. */
	bool FitsEquipmentSlot(const UEquipmentDataAsset* Item, int32 EquipmentSlotIndex) const;

	/** Where Item goes when no slot is given: the first empty fitting slot, else the first fitting one. -1 if none. */
	int32 FindEquipmentSlotFor(const UEquipmentDataAsset* Item) const;

	void BroadcastSlotChanged(int32 SlotIndex);

	/** Would using Item change anything right now (something to restore, or a buff)? */
	bool WouldConsumableHaveEffect(const UConsumableDataAsset* Item) const;

	UFUNCTION()
	void HandleCooldownStarted(FGameplayTag CooldownTag, float Duration);

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;

	/** Holds item cooldowns next to ability cooldowns, so cooldown reduction and UI work the same for both. */
	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<UResourceComponent> ResourceComponent;

	/** Asked before a weapon slot changes, so the drawn weapon isn't swapped mid-ability. */
	UPROPERTY(Transient)
	TObjectPtr<UWeaponManagerComponent> WeaponManager;

	UPROPERTY(Transient)
	TArray<FInventorySlot> Slots;

	/** Parallel to EquipmentSlots. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEquipmentDataAsset>> Equipped;

	/** Registered by the character: part tag -> its mesh component and default mesh. */
	UPROPERTY(Transient)
	TMap<FGameplayTag, FRegisteredEquipmentMesh> EquipmentMeshes;
};
