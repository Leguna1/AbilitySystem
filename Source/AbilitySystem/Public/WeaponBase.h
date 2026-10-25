#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactGroupTypes.h"
#include "WeaponBase.generated.h"

class ACharacter;
class UFeedbackComponent;
class UMeshComponent;
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

	/* -------------------- Projectiles -------------------- */

	/**
	 * Where projectile ProjectileIndex leaves this weapon (a staff tip, a blade), cycling through
	 * Launch Socket Names. False when the weapon has no launch sockets.
	 */
	UFUNCTION(BlueprintPure, Category = "Weapon|Projectiles")
	bool GetLaunchTransform(int32 ProjectileIndex, FTransform& OutTransform) const;

	UFUNCTION(BlueprintPure, Category = "Weapon|Projectiles")
	bool HasLaunchSockets() const { return !LaunchSocketNames.IsEmpty(); }

	/** The mesh launch sockets are looked up on. Default: the first mesh component on the weapon. */
	virtual UMeshComponent* GetWeaponMesh() const;

	/* -------------------- Feedback -------------------- */

	/** This weapon's look and sound. Set its Feedback Set in the Feedback component. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Feedback")
	UFeedbackComponent* GetFeedback() const { return FeedbackComponent; }

	/**
	 * Plays Cue.Weapon.Hit (target took the hit) or Cue.Weapon.Miss at StrikeLocation, through
	 * the ability's impact group if one is given. Resolve the location before delivering the payload.
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Feedback", meta = (AutoCreateRefTerm = "Hit,Group"))
	void PlayHitFeedback(AActor* HitActor, const FHitResult& Hit, FVector StrikeLocation, bool bHitTarget, const FImpactGroupHandle& Group);

	/** Where a strike from this weapon on HitActor landed. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Feedback", meta = (AutoCreateRefTerm = "Hit"))
	FVector ResolveStrikeLocation(const AActor* HitActor, const FHitResult& Hit) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<UFeedbackComponent> FeedbackComponent;

	/** Sockets on the weapon mesh that projectiles launch from (e.g. StaffTip). Empty = not a launcher. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectiles")
	TArray<FName> LaunchSocketNames;

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