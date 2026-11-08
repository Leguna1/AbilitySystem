#pragma once

#include "CoreMinimal.h"
#include "EquipmentDataAsset.h"
#include "WeaponDataAsset.generated.h"

class AWeaponBase;
class UAbility;

/**
 * Defines one wieldable weapon: what spawns, where it sits, and what it grants.
 *
 * It is also an inventory item (Equipment): it lies in the bag, drops as loot and is equipped into a weapon
 * slot like gear. Set Equipment Slot to Equipment.Slot.Weapon to fit every weapon slot (Primary, Secondary),
 * or to one of them to fit only that one. Display Name and Icon come from the item settings.
 *
 * While it sits in a weapon slot, the Weapon Manager keeps its actor spawned (holstered, or drawn), and its
 * Stat Modifiers and Granted Tags apply like any equipment. The Ability Kit and Granted Owner Tags below
 * apply only while it is drawn.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UWeaponDataAsset : public UEquipmentDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AWeaponBase> WeaponClass;

	/** Wielder mesh socket used while the weapon is drawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
	FName EquipSocketName = NAME_None;

	/** Wielder mesh socket used while holstered. None = hidden while holstered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attachment")
	FName HolsterSocketName = NAME_None;

	/** Abilities granted while this weapon is drawn, in hotbar order. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Abilities")
	TArray<TSubclassOf<UAbility>> AbilityKit;

	/** Loose owner tags applied while drawn (e.g. Weapon.Type.Bow). For tags while merely equipped, use Granted Tags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Abilities")
	FGameplayTagContainer GrantedOwnerTags;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
