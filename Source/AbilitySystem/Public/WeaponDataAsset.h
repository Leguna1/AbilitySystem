#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "WeaponDataAsset.generated.h"

class AWeaponBase;
class UAbility;
class UTexture2D;

/** Defines one wieldable weapon: what spawns, where it sits, and what it grants. */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UWeaponDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	TObjectPtr<UTexture2D> Icon;

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

	/** Loose owner tags applied while drawn (e.g. Weapon.Type.Bow). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Abilities")
	FGameplayTagContainer GrantedOwnerTags;
};