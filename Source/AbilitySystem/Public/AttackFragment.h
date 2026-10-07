#pragma once

#include "CoreMinimal.h"
#include "AbilityFragment.h"
#include "AttackFragment.generated.h"

class UOffensiveAbilityBase;
struct FHitSpec;

/** A feature for any attack, ranged or melee. Acts on what each hit carries. */
UCLASS(Abstract)
class ABILITYSYSTEM_API UAttackFragment : public UAbilityFragment
{
	GENERATED_BODY()

public:
	/** Adjusts what every hit of this execution carries. Runs after the ability has filled it in. */
	virtual void ModifyHitSpec(const UOffensiveAbilityBase& Ability, FHitSpec& Hit) const {}
};