// WeaponComponent.h
#pragma once

#include "CoreMinimal.h"
#include "SwordBase.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class ACharacter;
class ASwordBase;

/**
 * Equips and manages a melee weapon actor, mirroring UBowComponent. Spawns the
 * configured sword class, attaches it to a socket on the character mesh, and
 * exposes hit-detection toggles the melee ability drives during a swing.
 */
UCLASS(ClassGroup = (Weapon), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipWeapon();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void UnequipWeapon();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	ASwordBase* GetWeapon() const { return Weapon; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasEquippedWeapon() const { return IsValid(Weapon); }

	/** Enable the sword's hit detection for the active frames of a swing. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void BeginWeaponHitDetection();

	/** Disable the sword's hit detection. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void EndWeaponHitDetection();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Configuration")
	TSubclassOf<ASwordBase> WeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Configuration")
	FName WeaponSocketName = NAME_None;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<ASwordBase> Weapon;
};