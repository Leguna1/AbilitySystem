#pragma once

#include "CoreMinimal.h"
#include "OffensiveAbilityBase.h"
#include "AbilitySystemTags.h"
#include "ArrowShotParams.h"
#include "RangedAttackAbility.generated.h"

class AProjectileBase;
class ABowBase;
class UFeedbackSet;
class UProjectileDataAsset;

/** Where a ranged ability's projectiles come from. */
UENUM(BlueprintType)
enum class EProjectileLaunchSource : uint8
{
	/** Arrows are taken in hand, nocked on the equipped bow and released from it. Needs a drawn bow. */
	Bow UMETA(DisplayName = "Bow (Nocked Arrows)"),

	/**
	 * Projectiles appear at a launch point and fly: the equipped weapon's launch sockets
	 * (a staff tip, a blade), else sockets on the character (a hand). No weapon needed.
	 */
	LaunchPoint UMETA(DisplayName = "Launch Point")
};

/**
 * Fires projectiles: arrows from a bow, bolts from a staff or a hand, energy waves from a blade.
 * The montage's events drive it (prepare and nock for bows, then release). Projectiles come
 * from the world's projectile pool and are aimed at the ability target when there is one.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API URangedAttackAbility : public UOffensiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;

	virtual FGameplayTag GetCostEventTag() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	ABowBase* GetBow() const { return Bow; }

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool HasPreparedProjectile() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool HasReleasedProjectileThisCycle() const { return bProjectileReleased; }

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool IsProjectileNocked() const { return bProjectileNocked; }

	/** Projectiles this execution releases, fixed at activation (rank + modifiers; bows clamp to their sockets). */
	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	int32 GetProjectileCount() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	EProjectileLaunchSource GetLaunchSource() const { return LaunchSource; }

protected:
	UFUNCTION(BlueprintCallable, Category = "Ability|Ranged")
	void ResetProjectileCycle();

	UFUNCTION(BlueprintCallable, Category = "Ability|Ranged")
	void DiscardPreparedProjectile();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	bool PrepareProjectile();
	virtual bool PrepareProjectile_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	bool NockProjectile();
	virtual bool NockProjectile_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	bool ReleaseProjectile();
	virtual bool ReleaseProjectile_Implementation();

	/**
	 * Fallback direction used when this ability does not use the current
	 * target or when no target is available.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged")
	FVector ResolveProjectileDirection() const;
	virtual FVector ResolveProjectileDirection_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged")
	FVector ResolveProjectileDirectionForIndex(int32 ProjectileIndex) const;
	virtual FVector ResolveProjectileDirectionForIndex_Implementation(int32 ProjectileIndex) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged")
	float ResolveProjectileStrength() const;
	virtual float ResolveProjectileStrength_Implementation() const;

	/**
	 * Damage of each projectile this release fires. Default: Damage By Rank with Stat.Damage modifiers,
	 * fixed at activation. Override to scale it (e.g. Charged Shot by charge).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged")
	float ResolveProjectileDamage() const;
	virtual float ResolveProjectileDamage_Implementation() const;

	/**
	 * Whether projectiles are aimed at the ability target (the player's targeting, or the AI's target).
	 * Override and return false for abilities with custom aiming behavior, such as Volley Shot.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged|Targeting")
	bool ShouldUseCurrentTarget() const;
	virtual bool ShouldUseCurrentTarget_Implementation() const;

	/** Where projectile ProjectileIndex starts: weapon launch socket, else character socket, else in front of the eyes. */
	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	FTransform ResolveLaunchTransform(int32 ProjectileIndex) const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	void OnProjectilePrepared();
	virtual void OnProjectilePrepared_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	void OnProjectileNocked();
	virtual void OnProjectileNocked_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	void OnProjectileReleased(float Strength);
	virtual void OnProjectileReleased_Implementation(float Strength);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	void OnRangedAttackFinished(EAbilityEndReason EndReason, bool bHadPreparedProjectile, bool bReleasedProjectile);
	virtual void OnRangedAttackFinished_Implementation(EAbilityEndReason EndReason, bool bHadPreparedProjectile, bool bReleasedProjectile);

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	int32 GetReleasedProjectileCount() const { return ReleasedProjectiles.Num(); }

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	AProjectileBase* GetReleasedProjectile(int32 ProjectileIndex) const;

	/* -------------------- Projectile -------------------- */

	/** What is fired: an arrow, a bolt, an orb. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	TObjectPtr<UProjectileDataAsset> ProjectileData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	EProjectileLaunchSource LaunchSource = EProjectileLaunchSource::Bow;

	/** Hand sockets the arrows are held at before nocking, one per arrow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Bow",
		meta = (EditCondition = "LaunchSource == EProjectileLaunchSource::Bow", EditConditionHides))
	TArray<FName> ProjectileHandSocketNames;

	/** Bow sockets the arrows are nocked at, one per hand socket. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Bow",
		meta = (EditCondition = "LaunchSource == EProjectileLaunchSource::Bow", EditConditionHides))
	TArray<FName> ProjectileBowSocketNames;

	/**
	 * Character mesh sockets projectiles launch from (a hand, a staff-tip socket), cycled per projectile.
	 * Used only when the equipped weapon has no launch sockets. Empty = in front of the eyes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Launch Point",
		meta = (EditCondition = "LaunchSource == EProjectileLaunchSource::LaunchPoint", EditConditionHides))
	TArray<FName> CharacterLaunchSocketNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultProjectileStrength = 1.0f;

	/**
	 * This ability's arrow look, over the projectile data's own set: only what differs
	 * (a fiery Flight and Hit for Charged Shot). Empty = the projectile's look.
	 * Features can swap it per shot (Final Shot).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	TObjectPtr<UFeedbackSet> ProjectileFeedback;

	/* -------------------- Events -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events",
		meta = (EditCondition = "LaunchSource == EProjectileLaunchSource::Bow", EditConditionHides))
	FGameplayTag PrepareProjectileEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events",
		meta = (EditCondition = "LaunchSource == EProjectileLaunchSource::Bow", EditConditionHides))
	FGameplayTag NockProjectileEventTag;

	/** Anim event that fires the projectiles. Required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events")
	FGameplayTag ReleaseProjectileEventTag;

	/* -------------------- Rank -------------------- */

	/**
	 * Damage of each projectile, by rank: a multiple of the owner's Attack Power (or Magic Power
	 * for magic damage), 1 = 100%. Flat damage if the owner has no power.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	FAbilityRankedFloat DamageByRank = FAbilityRankedFloat(1.0f);

	/** Projectiles per release, by rank. Empty = one per bow socket pair (Bow) or one (Launch Point). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	FAbilityRankedInt ProjectileCountByRank;

private:
	bool HandleProjectileAnimationEvent(FGameplayTag EventTag);

	/** Spawns, places and fires one projectile per start point. Fills ReleasedProjectiles. */
	bool LaunchFromPoints(const TArray<FTransform>& StartTransforms, const TArray<FVector>& Directions, const FArrowShotParams& ShotParams);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AProjectileBase>> ReleasedProjectiles;

	UPROPERTY(Transient)
	bool bProjectilePrepared = false;

	UPROPERTY(Transient)
	bool bProjectileNocked = false;

	UPROPERTY(Transient)
	bool bProjectileReleased = false;

	UPROPERTY(Transient)
	TObjectPtr<ABowBase> Bow;

	int32 ResolveProjectileCount() const;

	UPROPERTY(Transient)
	float ResolvedDamage = 1.0f;

	UPROPERTY(Transient)
	int32 ResolvedProjectileCount = 0;

	/** Hands the projectiles of the last release to projectile features. */
	void NotifyFragmentsProjectilesReleased();
};
