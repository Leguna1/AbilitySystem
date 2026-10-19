#include "FinalShotFragment.h"

#include "AbilityComponent.h"
#include "ArrowShotParams.h"
#include "RapidFireAbility.h"

void UFinalShotFragment::ModifyShotParams(const URangedAttackAbility& Ability, FArrowShotParams& ShotParams) const
{
	const URapidFireAbility* RapidFire = Cast<URapidFireAbility>(&Ability);
	const UAbilityComponent* Component = Ability.GetAbilityComponent();

	if (!IsValid(RapidFire) ||
		!RapidFire->IsFinalShotPending() ||
		!IsValid(Component) ||
		!Component->HasAllOwnerTags(RequiredOwnerTags))
	{
		return;
	}

	ShotParams.Damage *= DamageMultiplier;
	ShotParams.Hit.Impact.PoiseDamage *= PoiseMultiplier;
	ShotParams.PierceCount += ExtraPierceCount;

	if (!Statuses.IsEmpty())
	{
		TArray<FStatusApplication> Extra;
		Ability.ResolveStatusSpecs(Statuses, Extra);
		ShotParams.Hit.Statuses.Append(Extra);
	}
}
