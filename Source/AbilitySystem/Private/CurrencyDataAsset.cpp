#include "CurrencyDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "CurrencyDataAsset"

UCurrencyDataAsset::UCurrencyDataAsset()
{
	Category = EItemCategory::Currency;

	// Gold drops on most kills; each loot entry can still scale or override this.
	DropChance = 100.0f;
}

#if WITH_EDITOR
bool UCurrencyDataAsset::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	// Category is fixed by the class, and stack size means nothing for money that lives outside the bag.
	const FName PropertyName = InProperty ? InProperty->GetFName() : NAME_None;
	return PropertyName != GET_MEMBER_NAME_CHECKED(UCurrencyDataAsset, Category)
		&& PropertyName != GET_MEMBER_NAME_CHECKED(UCurrencyDataAsset, MaxStackSize);
}

EDataValidationResult UCurrencyDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Category != EItemCategory::Currency)
	{
		Context.AddError(LOCTEXT("WrongCategory", "Currencies must have Category Currency."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
