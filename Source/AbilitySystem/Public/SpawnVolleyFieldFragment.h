#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HitReactionTypes.h"
#include "ProjectileFragment.h"
#include "SpawnVolleyFieldFragment.generated.h"

class AArrowFieldHazard;

/**
 * Leaves a circular field where a volley lands (Storm Volley): it slows or otherwise affects
 * everyone inside, using the statuses on the Field Class Blueprint.
 *
 * Size follows the volley's landing radius, so Stat.Radius grows it; Stat.HazardDuration lengthens it.
 * Thunderclap: while the owner has Thunderclap Required Owner Tags, the end of the rain hits
 * everyone in the field with this ability's poise damage x Thunderclap Poise Multiplier.
 */
UCLASS(meta = (DisplayName = "Spawn Volley Field"))
class ABILITYSYSTEM_API USpawnVolleyFieldFragment : public UProjectileFragment
{
	GENERATED_BODY()

public:
	virtual void OnProjectilesReleased(URangedAttackAbility& Ability, TConstArrayView<AProjectileBase*> Projectiles) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** The field to spawn. Its Blueprint holds the statuses, duration and visuals. */
	UPROPERTY(EditDefaultsOnly, Category = "Field")
	TSubclassOf<AArrowFieldHazard> FieldClass;

	/** Spawns only while the owner has all of these tags (e.g. Mod.VolleyShot.Storm from the path). Empty = every release. */
	UPROPERTY(EditDefaultsOnly, Category = "Field")
	FGameplayTagContainer RequiredOwnerTags;

	/** Seconds to wait for this release's arrows to land. Later landings are ignored. */
	UPROPERTY(EditDefaultsOnly, Category = "Field", meta = (ClampMin = "0.5"))
	float LandingWindow = 4.0f;

	/** Thunderclap happens only while the owner has all of these (e.g. Mod.VolleyShot.Thunderclap). Empty = never. */
	UPROPERTY(EditDefaultsOnly, Category = "Field|Thunderclap")
	FGameplayTagContainer ThunderclapRequiredOwnerTags;

	/** Thunderclap poise = this ability's poise damage (after Stat.PoiseDamage) x this. */
	UPROPERTY(EditDefaultsOnly, Category = "Field|Thunderclap", meta = (ClampMin = "0.0"))
	float ThunderclapPoiseMultiplier = 3.0f;

	/** Reaction the Thunderclap asks for. Poise still decides whether it happens. */
	UPROPERTY(EditDefaultsOnly, Category = "Field|Thunderclap")
	EHitReactionType ThunderclapReaction = EHitReactionType::Stagger;
};
