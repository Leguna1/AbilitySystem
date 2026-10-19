#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ModifierTypes.h"
#include "ProjectileFragment.h"
#include "HitCountStatusFragment.generated.h"

class AHitCountTracker;

/**
 * Every Hits Required hits on one target, it gets these statuses.
 * Per release (default): only the projectiles of ONE release count, e.g. Pinning Rain (3 volley arrows)
 * or Shredding Fan (3 fan arrows).
 * Count Across Releases: hits add up over consecutive releases while each new release comes within
 * Counting Window of the last, e.g. Pinning Fire (5 Rapid Fire hits).
 */
UCLASS(meta = (DisplayName = "Status On Repeated Hits"))
class ABILITYSYSTEM_API UHitCountStatusFragment : public UProjectileFragment
{
	GENERATED_BODY()

public:
	virtual void OnProjectilesReleased(URangedAttackAbility& Ability, TConstArrayView<AProjectileBase*> Projectiles) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** Hits from the same release needed on one target. */
	UPROPERTY(EditDefaultsOnly, Category = "Repeated Hits", meta = (ClampMin = "2"))
	int32 HitsRequired = 3;

	/** Applied to a target that reaches Hits Required, at this ability's rank. Once per target per release. */
	UPROPERTY(EditDefaultsOnly, Category = "Repeated Hits")
	TArray<FStatusEffectSpec> Statuses;

	/** Works only while the owner has all of these tags (e.g. Mod.VolleyShot.Pinning from the upgrade). Empty = always. */
	UPROPERTY(EditDefaultsOnly, Category = "Repeated Hits")
	FGameplayTagContainer RequiredOwnerTags;

	/** Seconds this release's projectiles are counted for. With Count Across Releases, every release restarts it. */
	UPROPERTY(EditDefaultsOnly, Category = "Repeated Hits", meta = (ClampMin = "0.5"))
	float CountingWindow = 4.0f;

	/** Hits add up across releases (one arrow at a time, like Rapid Fire) instead of per release. */
	UPROPERTY(EditDefaultsOnly, Category = "Repeated Hits")
	bool bCountAcrossReleases = false;

private:
	/** The running count while releases keep coming (Count Across Releases). */
	mutable TWeakObjectPtr<AHitCountTracker> SharedTracker;
};

/** Counts one release's hits per target. Spawned by Status On Repeated Hits; removes itself after its window. */
UCLASS(NotBlueprintable, NotPlaceable)
class ABILITYSYSTEM_API AHitCountTracker : public AActor
{
	GENERATED_BODY()

public:
	AHitCountTracker();

	void InitializeTracker(int32 InHitsRequired, const TArray<FStatusApplication>& InStatuses, float Window);
	void RegisterProjectile(AProjectileBase* Arrow);

	/** Keeps counting for another Window seconds, with this release's resolved statuses. */
	void ExtendTracker(const TArray<FStatusApplication>& InStatuses, float Window);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleArrowHit(AProjectileBase* Arrow, AActor* HitActor, float Damage, const FHitResult& Hit);

	void UnbindAll();

	int32 HitsRequired = 3;
	TArray<FStatusApplication> Statuses;

	TMap<TWeakObjectPtr<AProjectileBase>, uint32> TrackedArrows;
	TMap<TWeakObjectPtr<AActor>, int32> HitCounts;
};
