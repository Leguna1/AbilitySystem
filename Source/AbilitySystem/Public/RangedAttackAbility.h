#pragma once

#include "CoreMinimal.h"
#include "OffensiveAbilityBase.h"
#include "ImpactGroupTypes.h"
#include "AbilitySystemTags.h"
#include "RangedAttackAbility.generated.h"

class AArrowBase;
class ABowBase;
class UArrowDataAsset;

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

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	ABowBase* GetBow() const { return Bow; }

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool HasPreparedProjectile() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool HasReleasedProjectileThisCycle() const { return bProjectileReleased; }

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool IsProjectileNocked() const { return bProjectileNocked; }
	
	/** Arrows this execution prepares and releases, fixed at activation (rank + modifiers, clamped to sockets). */
	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	int32 GetProjectileCount() const;

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
	
	/** The ability's damage multiplier for each arrow it releases. Default 1. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged")
	float ResolveProjectileDamageMultiplier() const;
	virtual float ResolveProjectileDamageMultiplier_Implementation() const;

	/**
	 * Determines whether this ranged ability should use the current automatic
	 * target when releasing its projectile.
	 *
	 * Override and return false for abilities with custom aiming behavior,
	 * such as Volley Shot.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged|Targeting")
	bool ShouldUseCurrentTarget() const;
	virtual bool ShouldUseCurrentTarget_Implementation() const;

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
	AArrowBase* GetReleasedProjectile(int32 ProjectileIndex) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	TObjectPtr<UArrowDataAsset> ArrowData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Hand")
	TArray<FName> ProjectileHandSocketNames;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Bow")
	TArray<FName> ProjectileBowSocketNames;
	

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultProjectileStrength = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events")
	FGameplayTag PrepareProjectileEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events")
	FGameplayTag NockProjectileEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events")
	FGameplayTag ReleaseProjectileEventTag;
	
	/** Merge this ability's impact sounds: e.g. one hit and one miss sound per fan volley instead of one per arrow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Impact Audio")
	bool bGroupProjectileImpacts = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Impact Audio", meta = (EditCondition = "bGroupProjectileImpacts"))
	FImpactGroupSettings ProjectileImpactGroup;
	
	/** The ability's damage multiplier per rank, applied on top of the arrow data's damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	FAbilityRankedFloat DamageMultiplierByRank = FAbilityRankedFloat(1.0f);

	/** Arrows per release, by rank. Empty = one per configured socket pair. Always clamped to the socket count. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	FAbilityRankedInt ProjectileCountByRank;

private:
	bool HandleProjectileAnimationEvent(FGameplayTag EventTag);
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<AArrowBase>> ReleasedProjectiles;
	
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
	float ResolvedDamageMultiplier = 1.0f;

	UPROPERTY(Transient)
	int32 ResolvedProjectileCount = 0;
	
	/** Hands the arrows of the last release to projectile features. */
	void NotifyFragmentsProjectilesReleased();
};