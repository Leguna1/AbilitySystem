#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class ACharacter;
class USkeletalMeshComponent;
class UWeaponDataAsset;

/**
 * Base for every wieldable weapon. Owns the wielder relationship and the
 * drawn/holstered lifecycle. Subclasses own their meshes and mechanics, and
 * keep their own root component so existing Blueprint children are untouched.
 *
 * Initialize/Draw/Holster are driven by UWeaponManagerComponent.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	/** Stores wielder references. Does not change placement. */
	void InitializeWeapon(ACharacter* InWielder, UWeaponDataAsset* InWeaponData);

	/** Attaches to the equip socket and becomes the active weapon. */
	void HandleDrawn();

	/** Runs OnHolstered if drawn, then attaches to the holster socket or hides. */
	void HandleHolstered();

	UFUNCTION(BlueprintPure, Category = "Weapon")
	ACharacter* GetWielder() const { return Wielder; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	USkeletalMeshComponent* GetWielderMesh() const { return WielderMesh; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	UWeaponDataAsset* GetWeaponData() const { return WeaponData; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsDrawn() const { return bDrawn; }

	/** True for the wielder and anything in its ownership chain (weapons, arrows). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsWielderOwned(const AActor* Actor) const;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnDrawn();
	virtual void OnDrawn_Implementation();

	/** Stop anything in progress: hit windows, prepared arrows, looping feedback. */
	UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
	void OnHolstered();
	virtual void OnHolstered_Implementation();

private:
	bool AttachToWielderSocket(FName SocketName);
	void ApplyHolsteredPlacement();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> Wielder;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> WielderMesh;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponDataAsset> WeaponData;

	bool bDrawn = false;
};