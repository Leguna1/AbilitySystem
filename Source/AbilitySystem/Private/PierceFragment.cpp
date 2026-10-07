#include "PierceFragment.h"

#include "AbilitySystemTags.h"
#include "ArrowShotParams.h"
#include "RangedAttackAbility.h"

void UPierceFragment::ModifyShotParams(const URangedAttackAbility& Ability, FArrowShotParams& ShotParams) const
{
	// This feature supplies the base values; passives still scale them through stats.
	ShotParams.PierceCount = FMath::Max(Ability.GetModifiedInt(AbilitySystemTags::Stat_PierceCount, PierceCount), 0);
	ShotParams.PierceDamageFactor = FMath::Max(Ability.GetModifiedFloat(AbilitySystemTags::Stat_PierceDamage, DamageFactor), 0.0f);
}