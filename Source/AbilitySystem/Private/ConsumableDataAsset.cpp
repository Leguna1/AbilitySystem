#include "ConsumableDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ConsumableDataAsset"

UConsumableDataAsset::UConsumableDataAsset()
{
	Category = EItemCategory::Consumable;

	// Potions stack by default; set 1 for one-of-a-kind consumables.
	MaxStackSize = 10;
}

#if WITH_EDITOR
bool UConsumableDataAsset::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	// Fixed by the class: greyed out in the details panel.
	return !InProperty || InProperty->GetFName() != GET_MEMBER_NAME_CHECKED(UConsumableDataAsset, Category);
}

EDataValidationResult UConsumableDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Category != EItemCategory::Consumable)
	{
		Context.AddError(LOCTEXT("WrongCategory", "Consumables must have Category Consumable."));
		Result = EDataValidationResult::Invalid;
	}

	const bool bRestoresAnything = Restores.ContainsByPredicate([](const FResourceRestore& Restore)
	{
		return Restore.Amount > 0.0f || Restore.PercentOfMax > 0.0f;
	});

	if (!bRestoresAnything && !HasTimedEffect())
	{
		Context.AddWarning(LOCTEXT("DoesNothing", "Restores nothing and has no timed effect: it can never be used."));
	}

	if (EffectDuration <= 0.0f && (!StatModifiers.IsEmpty() || !GrantedTags.IsEmpty()))
	{
		Context.AddWarning(LOCTEXT("NoEffectDuration", "Stat Modifiers or Granted Tags are set but Effect Duration is 0: they are never applied."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
