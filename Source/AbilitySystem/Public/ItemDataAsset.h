#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ItemDataAsset.generated.h"

class UStaticMesh;
class UTexture2D;

/** What kind of item this is. Decides what the inventory lets you do with it (equip, ...). */
UENUM(BlueprintType)
enum class EItemCategory : uint8
{
	/** Worn on the character: goes into an equipment slot and changes its look and stats. */
	Equipment UMETA(DisplayName = "Equipment"),

	/** Used up from the bag (a potion): each use spends one and starts its cooldown. */
	Consumable UMETA(DisplayName = "Consumable"),

	// New categories go below, so saved assets keep their values.
};

/**
 * The data of one kind of item: what it is called, how it looks in the UI, how it stacks.
 * Inventories hold references to these assets; the asset itself never changes at runtime.
 *
 * Children add what their category needs (Equipment Data Asset: slot, meshes, stats;
 * Consumable Data Asset: what using it does, its cooldown).
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Every item, whatever its class, is one Asset Manager type ("Item"), named by Item Id when set. */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UFUNCTION(BlueprintPure, Category = "Item")
	FGameplayTag GetItemId() const { return ItemId; }

	UFUNCTION(BlueprintPure, Category = "Item")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Item")
	FText GetDescription() const { return Description; }

	UFUNCTION(BlueprintPure, Category = "Item")
	UTexture2D* GetIcon() const { return Icon; }

	UFUNCTION(BlueprintPure, Category = "Item")
	EItemCategory GetCategory() const { return Category; }

	/** How many fit in one inventory slot. Always at least 1. */
	UFUNCTION(BlueprintPure, Category = "Item")
	int32 GetMaxStackSize() const { return FMath::Max(MaxStackSize, 1); }

	/** Percent chance to drop when a loot list has this item, before that entry's own changes. */
	UFUNCTION(BlueprintPure, Category = "Item|Loot")
	float GetDropChance() const { return DropChance; }

	/** Mesh shown on the pickup lying on the ground. Null = the pickup Blueprint keeps its own. */
	UFUNCTION(BlueprintPure, Category = "Item|Loot")
	UStaticMesh* GetPickupMesh() const { return PickupMesh; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	static const FPrimaryAssetType ItemAssetType;

protected:
	/** Unique id of this item, e.g. Item.Equipment.LeatherBoots. Saves and quests refer to it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (Categories = "Item"))
	FGameplayTag ItemId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** Shown in the item tooltip. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	/** Shown in inventory and equipment slots. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon;

	/** Set by the item's class (an Equipment Data Asset is always Equipment), so it is greyed out there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	EItemCategory Category = EItemCategory::Equipment;

	/** How many fit in one inventory slot. 1 = never stacks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = "1"))
	int32 MaxStackSize = 1;

	/**
	 * How likely this item is to drop, in percent, from anything whose loot list has it.
	 * This is the item's own rarity: a common pelt 40, a rare ring 2. Each loot entry can scale it
	 * (wolves drop pelts twice as often) or replace it outright (a boss always drops its key).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Loot", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "Percent"))
	float DropChance = 10.0f;

	/** Shown spinning on the pickup when the item lies on the ground. Empty = the pickup Blueprint's own mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Loot")
	TObjectPtr<UStaticMesh> PickupMesh;
};
