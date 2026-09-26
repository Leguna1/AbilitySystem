#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponBase.h"
#include "WeaponManagerComponent.generated.h"

class ACharacter;
class UAbilityComponent;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Loadout")
	TArray<TObjectPtr<UWeaponDataAsset>> Loadout;

	/** Loadout entry drawn at BeginPlay. -1 = start unarmed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Loadout", meta = (ClampMin = "-1"))
	int32 StartingLoadoutIndex = 0;
	
	const TArray<TObjectPtr<UWeaponDataAsset>>& GetLoadout() const { return Loadout; }

private:
	/** Null WeaponData = unarmed. */
	bool SwapTo(UWeaponDataAsset* WeaponData, int32 LoadoutIndex);

	AWeaponBase* FindCachedWeapon(const UWeaponDataAsset* WeaponData) const;
	AWeaponBase* FindOrSpawnWeapon(UWeaponDataAsset* WeaponData);

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

	int32 EquippedLoadoutIndex = INDEX_NONE;

	/** Re-entrancy guard: a cancelled ability's end callbacks must not start a second swap. */
	bool bSwapping = false;
};