#include "ApplyStatusOnHitFragment.h"

#include "OffensiveAbilityBase.h"
#include "PayloadReceiver.h"

#if WITH_EDITOR
#include "AbilityValidation.h"
#endif

void UApplyStatusOnHitFragment::ModifyHitSpec(const UOffensiveAbilityBase& Ability, FHitSpec& Hit) const
{
	TArray<FStatusApplication> Resolved;
	Ability.ResolveStatusSpecs(Statuses, Resolved);

	// Appended, so several features can add their own statuses to the same hit.
	Hit.Statuses.Append(Resolved);
}

#if WITH_EDITOR
EDataValidationResult UApplyStatusOnHitFragment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Statuses.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("ApplyStatusOnHit", "Empty", "Apply Status on Hit has no statuses, so it does nothing. Add one or remove the feature."));
	}

	return CombineDataValidationResults(Result,
		AbilityValidation::ValidateStatusSpecs(Statuses, TEXT("Apply Status on Hit"), Context));
}
#endif