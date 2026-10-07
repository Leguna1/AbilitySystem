#include "ChanceBuffPassive.h"

namespace
{
	const FName BuffEntryKey(TEXT("Buff"));
}

void UChanceBuffPassive::OnGameplayEvent_Implementation(const FGameplayTag EventTag, UActiveAbility* Source)
{
	Super::OnGameplayEvent_Implementation(EventTag, Source);

	if (!TriggerEventTag.IsValid() || !EventTag.MatchesTagExact(TriggerEventTag))
	{
		return;
	}

	if (FMath::FRand() >= GetRankedFloat(ChanceByRank))
	{
		return;
	}

	// Same entry key every time: a proc while active refreshes the duration instead of stacking.
	ApplyTimedEffect(BuffEntryKey, BuffModifiers, BuffTags, GetRankedFloat(DurationByRank));
}
#if WITH_EDITOR
#include "AbilityValidation.h"

EDataValidationResult UChanceBuffPassive::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	return CombineDataValidationResults(Result,
		AbilityValidation::ValidateModifiers(BuffModifiers, TEXT("Buff Modifiers"), Context));
}
#endif