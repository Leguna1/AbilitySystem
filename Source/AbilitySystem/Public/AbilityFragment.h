#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "UObject/Object.h"
#include "AbilityFragment.generated.h"

class UActiveAbility;

/**
 * An optional feature added to an ability through its Features list
 * (pierce, statuses on hit, hazards...).
 *
 * Rules, following Epic's GameplayEffect components:
 *  - Stateless: a fragment holds configuration only. Hooks are const;
 *    anything that changes during an execution lives on the ability or
 *    on what it spawns.
 *  - Few hooks: a fragment acts only at the moments its base class exposes.
 *  - Independent: fragments never talk to each other.
 *
 * Subclasses: give each a short DisplayName and a one-line class comment,
 * which is the tooltip people see when adding it.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ABILITYSYSTEM_API UAbilityFragment : public UObject
{
	GENERATED_BODY()

public:
	/** Before the ability's own activation runs. Always paired with OnExecutionEnded. */
	virtual void OnExecutionStarted(UActiveAbility& Ability) const {}

	/** After the ability's own end handling, for any end reason. */
	virtual void OnExecutionEnded(UActiveAbility& Ability, EAbilityEndReason EndReason) const {}

	/** Whether one ability may hold several of this fragment type. */
	virtual bool AllowsMultiple() const { return false; }
};