#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ProjectileFragment.h"
#include "SpawnHazardFragment.generated.h"

class AArrowLinkHazard;

/** Arrows that land link up into a ground hazard that affects anyone touching it (e.g. Storm Volley's lines). */
UCLASS(meta = (DisplayName = "Spawn Hazard"))
class ABILITYSYSTEM_API USpawnHazardFragment : public UProjectileFragment
{
	GENERATED_BODY()

public:
	virtual void OnProjectilesReleased(URangedAttackAbility& Ability, TConstArrayView<AArrowBase*> Projectiles) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** The hazard to spawn. Its Blueprint holds the statuses, duration and link distance. */
	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	TSubclassOf<AArrowLinkHazard> HazardClass;

	/** Spawns only while the owner has all of these tags (e.g. Mod.VolleyShot.Storm from a path). Empty = every release. */
	UPROPERTY(EditDefaultsOnly, Category = "Hazard")
	FGameplayTagContainer RequiredOwnerTags;

	/** Seconds to wait for this release's arrows to land. Later landings are ignored. */
	UPROPERTY(EditDefaultsOnly, Category = "Hazard", meta = (ClampMin = "0.5"))
	float LandingWindow = 4.0f;
};