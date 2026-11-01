#pragma once

#include "CoreMinimal.h"
#include "ItemDataAsset.h"
#include "ModifierTypes.h"
#include "EquipmentDataAsset.generated.h"

class USkeletalMesh;

/** One body part this equipment changes on the wearer. */
USTRUCT(BlueprintType)
struct FEquipmentMesh
{
	GENERATED_BODY()

	/**
	 * The body part to change, as the character registered it (Register Equipment Meshes).
	 * Empty = this item's own Equipment Slot.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment.Slot"))
	FGameplayTag Part;

	/** Shown on that component while equipped. Empty = the part is hidden (a hood hiding the hair). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<USkeletalMesh> Mesh;
};

/**
 * An item worn in an equipment slot. While equipped it swaps meshes on the character
 * and applies its stats; unequipping puts the character's own meshes back.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UEquipmentDataAsset : public UItemDataAsset
{
	GENERATED_BODY()

public:
	UEquipmentDataAsset();

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FGameplayTag GetEquipmentSlot() const { return EquipmentSlot; }

	const TArray<FEquipmentMesh>& GetMeshes() const { return Meshes; }
	const TArray<FStatModifier>& GetStatModifiers() const { return StatModifiers; }
	const FGameplayTagContainer& GetGrantedTags() const { return GrantedTags; }

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/** The slot it is worn in, e.g. Equipment.Slot.Feet. Must match one of the inventory's equipment slots. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment.Slot"))
	FGameplayTag EquipmentSlot;

	/**
	 * Meshes set on the wearer while equipped. Usually one entry with only Mesh set.
	 * Add more to change other parts too (armor that also replaces the arms, a hood hiding the hair).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Visuals")
	TArray<FEquipmentMesh> Meshes;

	/** Stat and attribute modifiers applied while equipped. Read at rank 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Stats")
	TArray<FStatModifier> StatModifiers;

	/** Owner tags while equipped (e.g. Equipment.Armor.Heavy for abilities or passives to check). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Stats")
	FGameplayTagContainer GrantedTags;
};
