#include "SelfBuffAbility.h"

#include "CombatantComponent.h"
#include "AbilityComponent.h"
#include "GameFramework/Character.h"

#if WITH_EDITOR
#include "AbilityValidation.h"
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SelfBuffAbility"

void USelfBuffAbility::ActivateAbility_Implementation()
{
	bBuffsApplied = false;
	Super::ActivateAbility_Implementation();
}

void USelfBuffAbility::OnAbilityCommitted_Implementation()
{
	Super::OnAbilityCommitted_Implementation();

	if (!ApplyBuffEventTag.IsValid())
	{
		ApplyBuffs();
	}
}

void USelfBuffAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (ApplyBuffEventTag.IsValid() && EventTag.MatchesTagExact(ApplyBuffEventTag))
	{
		ApplyBuffs();
	}
}

void USelfBuffAbility::ApplyBuffs_Implementation()
{
	if (bBuffsApplied)
	{
		return;
	}

	const ACharacter* Character = GetOwningCharacter();
	UCombatantComponent* Combatant = IsValid(Character) ? Character->FindComponentByClass<UCombatantComponent>() : nullptr;

	if (!IsValid(Combatant))
	{
		return;
	}

	bBuffsApplied = true;

	const int32 Rank = GetAbilityRank();
	const FGameplayTagContainer OwnerTags = IsValid(GetAbilityComponent())
		? GetAbilityComponent()->GetOwnedGameplayTags()
		: FGameplayTagContainer();

	// Every cast creates a new execution object; the class default is the same every time,
	// so casting again refreshes the buff instead of adding a second copy.
	const UObject* BuffSource = GetClass()->GetDefaultObject();

	for (const FStatusEffectSpec& Spec : Buffs)
	{
		if (!Spec.StatusTag.IsValid() || !OwnerTags.HasAll(Spec.RequiredOwnerTags))
		{
			continue;
		}

		FStatusApplication Application;
		Application.StatusTag = Spec.StatusTag;
		Application.Display = Spec.Display;
		Application.Modifiers = Spec.Modifiers;
		Application.GrantedTags = Spec.GrantedTags;
		Application.Rank = Rank;
		Application.Duration = FMath::Max(Spec.DurationByRank.Get(Rank), 0.0f);

		Combatant->ApplyStatus(Application, BuffSource);
	}

	OnBuffsApplied();
}

#if WITH_EDITOR
EDataValidationResult USelfBuffAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Buffs.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("NoBuffs", "Buffs is empty: casting does nothing but play the montage."));
	}

	for (int32 Index = 0; Index < Buffs.Num(); ++Index)
	{
		const FStatusEffectSpec& Spec = Buffs[Index];

		if (!Spec.StatusTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("NoTag", "Buffs [{0}]: no Status Tag. It identifies the buff, so casting again can refresh it."), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (!Spec.Display.IsSet())
		{
			Context.AddWarning(FText::Format(LOCTEXT("NoDisplay", "Buffs [{0}]: no Display name or icon, so it won't show on the effects bar."), Index));
		}

		Result = CombineDataValidationResults(Result,
			AbilityValidation::ValidateModifiers(Spec.Modifiers, *FString::Printf(TEXT("Buffs [%d]: Modifiers"), Index), Context));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
