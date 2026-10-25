#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ModifierTypes.h"
#include "ProjectileFragment.h"
#include "FinalShotFragment.generated.h"

class UFeedbackSet;

/**
 * Rapid Fire's last arrow of a full volley becomes a heavy shot: more damage, more poise,
 * extra pierce and optional statuses. A volley cut short by moving has no final shot.
 * Its look: Final Shot Feedback (only what differs, e.g. a heavier Flight and Hit).
 */
UCLASS(meta = (DisplayName = "Final Shot"))
class ABILITYSYSTEM_API UFinalShotFragment : public UProjectileFragment
{
	GENERATED_BODY()

public:
	virtual void ModifyShotParams(const URangedAttackAbility& Ability, FArrowShotParams& ShotParams) const override;

protected:
	/** Works only while the owner has all of these (e.g. Mod.RapidFire.Finale from the upgrade). Empty = always. */
	UPROPERTY(EditDefaultsOnly, Category = "Final Shot")
	FGameplayTagContainer RequiredOwnerTags;

	/** The final arrow's damage x this. */
	UPROPERTY(EditDefaultsOnly, Category = "Final Shot", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 3.0f;

	/** The final arrow's poise damage x this. */
	UPROPERTY(EditDefaultsOnly, Category = "Final Shot", meta = (ClampMin = "0.0"))
	float PoiseMultiplier = 2.0f;

	/** Extra living targets the final arrow passes through. */
	UPROPERTY(EditDefaultsOnly, Category = "Final Shot", meta = (ClampMin = "0"))
	int32 ExtraPierceCount = 2;

	/** Added to what the final arrow applies, at this ability's rank. */
	UPROPERTY(EditDefaultsOnly, Category = "Final Shot")
	TArray<FStatusEffectSpec> Statuses;

	/** The final arrow's look over the arrow's own set. Empty = looks like any other shot. */
	UPROPERTY(EditDefaultsOnly, Category = "Final Shot")
	TObjectPtr<UFeedbackSet> FinalShotFeedback;
};
