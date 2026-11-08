#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "WeaponBase.h"
#include "WeaponManagerComponent.generated.h"

class ACharacter;
class UAbilityComponent;
class UEquipmentDataAsset;
class UInventoryComponent;
class UWeaponDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWeaponChangedSignature, UWeaponDataAsset*, PreviousWeaponData, AWeaponBase*, NewWeapon);

/** A spawned weapon kept alive for reuse across swaps. */
USTRUCT()
struct FWeaponInstanceEntry
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UWeaponDataAsset> WeaponData;

	UPROPERTY()
	TObjectPtr<AWeaponBase> Weapon;
};

/**
 * Owns every weapon the character can wield: spawns each once, caches it,
 * draws/holsters on swap, and keeps the ability component's granted kit and
 * owner tags in sync with the drawn weapon.
 *
 * With Inventory Weapon Slots set, weapons are inventory items: the loadout is what the inventory has
 * equipped in those slots, and equipping or unequipping one there spawns or removes its actor.
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UWeaponManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponManagerComponent();

	/** Draws WeaponData (loadout or not). Returns true if it is drawn afterwards. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipWeapon(UWeaponDataAsset* WeaponData);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipLoadoutIndex(int32 LoadoutIndex);

	/** Steps through the loadout with wrap-around, skipping empty entries. From unarmed, +1 starts at index 0. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool CycleWeapon(int32 Direction = 1);

	/** Goes unarmed: revokes the kit and holsters the drawn weapon. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool HolsterWeapon();

	/** Weapons can only be swapped while no ability is active. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanSwapWeapon() const;

	/** False if SlotTag's weapon is the drawn one and it can't be swapped now. The inventory asks before changing a slot. */
	bool CanChangeWeaponSlot(const FGameplayTag& SlotTag) const;

	/** True when the loadout comes from the inventory's weapon slots. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool UsesInventoryLoadout() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	AWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

	template <typename T>
	T* GetEquippedWeaponAs() const { return Cast<T>(EquippedWeapon.Get()); }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UWeaponDataAsset* GetEquippedWeaponData() const
	{
		return IsValid(EquippedWeapon) ? EquippedWeapon->GetWeaponData() : nullptr;
	}

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasEquippedWeapon() const { return IsValid(EquippedWeapon); }

	/** INDEX_NONE when unarmed or when the drawn weapon is not from the loadout. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetEquippedLoadoutIndex() const { return EquippedLoadoutIndex; }

	/** Blueprint access to the loadout (returns a copy; UFUNCTIONs cannot expose TObjectPtr). */
	UFUNCTION(BlueprintPure, Category = "Weapon", meta = (DisplayName = "Get Loadout"))
	TArray<UWeaponDataAsset*> GetLoadoutForBlueprint() const { return ObjectPtrDecay(Loadout); }
	
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FWeaponChangedSignature OnWeaponChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * The weapons to cycle through, in order. Ignored when Inventory Weapon Slots is used: the loadout is
	 * then whatever is equipped in those slots. Fine as is for characters without an inventory (enemies).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Loadout")
	TArray<TObjectPtr<UWeaponDataAsset>> Loadout;

	/**
	 * Take the loadout from the owner's Inventory Component: one entry per tag, the weapon equipped in that
	 * equipment slot (e.g. Equipment.Slot.Weapon.Primary, Equipment.Slot.Weapon.Secondary). Each tag must
	 * also be one of the inventory's Equipment Slots. Equipped weapons spawn holstered; unequipped ones are removed.
	 * Put the starting weapons in the inventory's Starting Equipment. Empty = use Loadout.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Loadout", meta = (Categories = "Equipment.Slot"))
	TArray<FGameplayTag> InventoryWeaponSlots;

	/**
	 * Equipping a weapon into an Inventory Weapon Slot while unarmed draws it. Off = it goes to its holster
	 * (hidden if it has no Holster Socket Name) until drawn. Starting equipment follows Starting Loadout Index instead.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Loadout")
	bool bDrawOnEquipWhenUnarmed = true;

	/** Loadout entry drawn at start (with an inventory: as soon as that slot first has a weapon). -1 = start unarmed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Loadout", meta = (ClampMin = "-1"))
	int32 StartingLoadoutIndex = 0;
	
	const TArray<TObjectPtr<UWeaponDataAsset>>& GetLoadout() const { return Loadout; }

private:
	/** Null WeaponData = unarmed. */
	bool SwapTo(UWeaponDataAsset* WeaponData, int32 LoadoutIndex);

	AWeaponBase* FindCachedWeapon(const UWeaponDataAsset* WeaponData) const;
	AWeaponBase* FindOrSpawnWeapon(UWeaponDataAsset* WeaponData);

	UFUNCTION()
	void HandleEquipmentChanged(int32 EquipmentSlotIndex, UEquipmentDataAsset* PreviousItem, UEquipmentDataAsset* NewItem);

	/** Re-reads every inventory weapon slot. */
	void SyncLoadoutFromInventory();

	/** Puts WeaponData in loadout entry LoadoutIndex: draws it if that entry is drawn, else spawns it holstered. */
	void SetLoadoutEntry(int32 LoadoutIndex, UWeaponDataAsset* WeaponData);

	/** Destroys the cached actor of WeaponData unless it is still in the loadout or drawn. */
	void ReleaseWeaponIfUnused(const UWeaponDataAsset* WeaponData);

	/** Draws the starting entry once it has a weapon. */
	void TryStartingDraw();

	void GrantKit(const UWeaponDataAsset* WeaponData);
	void RevokeEquippedKit();
	void DestroyCachedWeapons();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<AWeaponBase> EquippedWeapon;

	UPROPERTY(Transient)
	TArray<FWeaponInstanceEntry> CachedWeapons;

	/** Set while the loadout mirrors this inventory's weapon slots. */
	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> BoundInventory;

	int32 EquippedLoadoutIndex = INDEX_NONE;

	/** The starting entry hasn't been drawn yet (with an inventory, its weapon may arrive after Begin Play). */
	bool bStartingDrawPending = false;

	/** False during the first frame, while the inventory equips its starting items. */
	bool bStartupEquipDone = false;

	/** Re-entrancy guard: a cancelled ability's end callbacks must not start a second swap. */
	bool bSwapping = false;
};