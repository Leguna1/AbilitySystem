#include "AbilityValidation.h"
#include "Algo/Find.h"

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

		// Read once for the whole character (walk speed, the damage formula), so a scope never matches.
		// Attack Speed isn't here: abilities read it with their own tags.
		static const FGameplayTag CharacterOnlyStats[] =
		{
			AbilitySystemTags::Stat_MoveSpeed,
			AbilitySystemTags::Stat_AttackPower,
			AbilitySystemTags::Stat_AttackPower_Min,
			AbilitySystemTags::Stat_AttackPower_Max,
			AbilitySystemTags::Stat_MagicPower,
			AbilitySystemTags::Stat_MagicPower_Min,
			AbilitySystemTags::Stat_MagicPower_Max,
			AbilitySystemTags::Stat_Armor,
			AbilitySystemTags::Stat_MagicResist,
			AbilitySystemTags::Stat_MaxHealth,
			AbilitySystemTags::Stat_MaxFocus
		};


		const bool bCharacterAttribute = Algo::Find(CharacterOnlyStats, Modifier.Stat) != nullptr;

		if (bCharacterAttribute && !Modifier.AbilityScope.IsEmpty())
		{
			Context.AddError(FText::Format(LOCTEXT("ScopedAttribute",
				"{0}: {1} belongs to the whole character, so a scoped modifier never applies. Clear Ability Scope, or use Stat.Damage for an ability-only bonus."),
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

