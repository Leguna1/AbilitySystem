#pragma once

#include "CoreMinimal.h"
#include "AttackFragment.h"
#include "ProjectileFragment.generated.h"

class URangedAttackAbility;
class AArrowBase;


struct FArrowShotParams;

/** A feature for ranged abilities. Acts on the shot each release carries. */
UCLASS(Abstract)
class ABILITYSYSTEM_API UProjectileFragment : public UAttackFragment
{
	GENERATED_BODY()

public:
	/** Adjusts the shot every arrow of a release carries. Runs after the ability has filled it in. */
	virtual void ModifyShotParams(const URangedAttackAbility& Ability, FArrowShotParams& ShotParams) const {}
	
	/** The arrows of a release just left the bow, with their shot params already applied. */
	virtual void OnProjectilesReleased(URangedAttackAbility& Ability, TConstArrayView<AArrowBase*> Projectiles) const {}
};