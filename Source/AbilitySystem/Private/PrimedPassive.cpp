#include "PrimedPassive.h"

#include "ActiveAbility.h"

namespace
{
	const FName PrimedEntryKey(TEXT("Primed"));
}

void UPrimedPassive::OnGameplayEvent_Implementation(const FGameplayTag EventTag, UActiveAbility* Source)
{
	Super::OnGameplayEvent_Implementation(EventTag, Source);

	if (!TriggerEventTag.IsValid() || !EventTag.MatchesTagExact(TriggerEventTag))
	{
		return;
	}

	RemainingCharges = FMath::Max(GetRankedInt(ChargesByRank), 1);
	ApplyTimedEffect(PrimedEntryKey, BonusModifiers, PrimedTags, GetRankedFloat(DurationByRank));
	SetTimedEffectStacks(PrimedEntryKey, RemainingCharges);
}

void UPrimedPassive::OnAbilityCommitted_Implementation(UActiveAbility* Ability)
{
	Super::OnAbilityCommitted_Implementation(Ability);

	if (RemainingCharges <= 0 || !IsValid(Ability))
	{
		return;
	}

	// The bonus may have expired on its own since it was applied.
	if (!HasTimedEffect(PrimedEntryKey))
	{
		RemainingCharges = 0;
		return;
	}

	if (!Ability->GetAbilityTags().HasAny(ConsumingAbilityTags))
	{
		return;
	}

	if (--RemainingCharges <= 0)
	{
		RemoveTimedEffect(PrimedEntryKey);
		return;
	}

	SetTimedEffectStacks(PrimedEntryKey, RemainingCharges);
}

void UPrimedPassive::OnPassiveDeactivated_Implementation()
{
	RemainingCharges = 0;
	Super::OnPassiveDeactivated_Implementation();
}
#if WITH_EDITOR
#include "AbilityValidation.h"

EDataValidationResult UPrimedPassive::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	return CombineDataValidationResults(Result,
		AbilityValidation::ValidateModifiers(BonusModifiers, TEXT("Bonus Modifiers"), Context));
}
#endif