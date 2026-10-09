#include "AbilityValidation.h"

#if WITH_EDITOR

#include "AbilitySystemTags.h"
#include "ModifierTypes.h"

#define LOCTEXT_NAMESPACE "AbilityValidation"

EDataValidationResult AbilityValidation::ValidateModifiers(
	const TArray<FStatModifier>& Modifiers,
	const FString& Location,
	FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	for (int32 Index = 0; Index < Modifiers.Num(); ++Index)
	{
		const FStatModifier& Modifier = Modifiers[Index];
		const FText Where = FText::FromString(FString::Printf(TEXT("%s [%d]"), *Location, Index));

		if (!Modifier.Stat.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("NoStat", "{0}: no Stat is set, so this modifier does nothing."), Where));
			Result = EDataValidationResult::Invalid;
		}

		if (Modifier.Magnitude.Values.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("NoMagnitude", "{0}: Magnitude has no values. Add at least one."), Where));
			Result = EDataValidationResult::Invalid;
		}

		// Character attributes are evaluated without ability tags, so a scope never matches.
		const bool bCharacterAttribute =
			Modifier.Stat == AbilitySystemTags::Stat_MoveSpeed ||
			Modifier.Stat == AbilitySystemTags::Stat_AttackSpeed;

		if (bCharacterAttribute && !Modifier.AbilityScope.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("ScopedAttribute",
				"{0}: {1} is a character attribute, so a scoped modifier never applies. Clear Ability Scope."),
				Where, FText::FromName(Modifier.Stat.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
		
	}

	return Result;
}

EDataValidationResult AbilityValidation::ValidateStatusSpecs(
	const TArray<FStatusEffectSpec>& Specs,
	const FString& Location,
	FDataValidationContext& Context)
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	for (int32 Index = 0; Index < Specs.Num(); ++Index)
	{
		const FStatusEffectSpec& Spec = Specs[Index];
		const FString SpecLocation = FString::Printf(TEXT("%s [%d]"), *Location, Index);
		const FText Where = FText::FromString(SpecLocation);

		if (!Spec.StatusTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("NoStatusTag", "{0}: no Status Tag is set, so this status is never applied."), Where));
			Result = EDataValidationResult::Invalid;
		}

		const bool bHasDuration = Spec.DurationByRank.Values.ContainsByPredicate([](const float Value) { return Value > 0.0f; });

		if (!bHasDuration)
		{
			Context.AddError(FText::Format(LOCTEXT("NoDuration",
				"{0}: Duration By Rank has no positive value. Statuses must be temporary, so this one would be skipped."), Where));
			Result = EDataValidationResult::Invalid;
		}

		Result = CombineDataValidationResults(Result,
			ValidateModifiers(Spec.Modifiers, SpecLocation + TEXT(" Modifiers"), Context));
		
		if (!Spec.Display.IsSet())
		{
			Context.AddWarning(FText::Format(LOCTEXT("NoDisplay",
				"{0}: no Display Name or Icon; it will show as its tag with a blank icon on effect bars."), Where));
		}
	}

	return Result;
}


#undef LOCTEXT_NAMESPACE

#endif

