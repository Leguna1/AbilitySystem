#include "ItemDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ItemDataAsset"

const FPrimaryAssetType UItemDataAsset::ItemAssetType(TEXT("Item"));

FPrimaryAssetId UItemDataAsset::GetPrimaryAssetId() const
{
	// One type for every item class, so a single Asset Manager rule finds them all.
	return FPrimaryAssetId(ItemAssetType, ItemId.IsValid() ? ItemId.GetTagName() : GetFName());
}

#if WITH_EDITOR
EDataValidationResult UItemDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!ItemId.IsValid())
	{
		Context.AddError(LOCTEXT("MissingItemId", "Item Id is not set."));
		Result = EDataValidationResult::Invalid;
	}

	if (DisplayName.IsEmpty())
	{
		Context.AddWarning(LOCTEXT("MissingDisplayName", "Display Name is empty."));
	}

	if (!Icon)
	{
		Context.AddWarning(LOCTEXT("MissingIcon", "Icon is not set: the item shows as an empty slot."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
