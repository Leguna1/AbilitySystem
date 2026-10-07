#pragma once

#include "CoreMinimal.h"
#include "ProjectileFragment.h"
#include "PierceFragment.generated.h"

/** Arrows pass through living targets, losing some damage with each one. */
UCLASS(meta = (DisplayName = "Pierce"))
class ABILITYSYSTEM_API UPierceFragment : public UProjectileFragment
{
	GENERATED_BODY()

public:
	virtual void ModifyShotParams(const URangedAttackAbility& Ability, FArrowShotParams& ShotParams) const override;

protected:
	/** Living targets each arrow passes through before stopping. Passives can raise it (Stat.PierceCount). */
	UPROPERTY(EditDefaultsOnly, Category = "Pierce", meta = (ClampMin = "1"))
	int32 PierceCount = 1;

	/** Damage kept per target passed through: 0.5 = each next target takes half. Passives can change it (Stat.PierceDamage). */
	UPROPERTY(EditDefaultsOnly, Category = "Pierce", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DamageFactor = 0.5f;
};