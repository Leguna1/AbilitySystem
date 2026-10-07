#pragma once

#include "CoreMinimal.h"
#include "AttackFragment.h"
#include "ModifierTypes.h"
#include "ApplyStatusOnHitFragment.generated.h"

/** Hits apply statuses to the target, e.g. a mark or a slow. Values scale with the ability's rank. */
UCLASS(meta = (DisplayName = "Apply Status on Hit"))
class ABILITYSYSTEM_API UApplyStatusOnHitFragment : public UAttackFragment
{
	GENERATED_BODY()

public:
	virtual void ModifyHitSpec(const UOffensiveAbilityBase& Ability, FHitSpec& Hit) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/**
	 * Applied by every hit. Values are read at the ability's rank, or at Rank Source Ability Id's rank if set.
	 * Re-applying the same Status Tag refreshes it instead of stacking.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Statuses")
	TArray<FStatusEffectSpec> Statuses;
};