#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ModifierTypes.h"
#include "ProjectileFragment.h"
#include "HitCountStatusFragment.generated.h"

/**
 * When one target is hit Hits Required times by the projectiles of ONE release, it gets these statuses.
 * Pinning Rain: 3 volley arrows on the same enemy pin it (Status.Pinned, Stat.MoveSpeed -100%).
 * Also fits Fan Shot (every arrow into one target) or a multi-shot Rapid Fire.
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

	/** Seconds this release's projectiles are counted for. */
	UPROPERTY(EditDefaultsOnly, Category = "Repeated Hits", meta = (ClampMin = "0.5"))
	float CountingWindow = 4.0f;
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
