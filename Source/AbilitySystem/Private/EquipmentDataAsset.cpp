#include "EquipmentDataAsset.h"

#include "Engine/SkeletalMesh.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "EquipmentDataAsset"

UEquipmentDataAsset::UEquipmentDataAsset()
{
	Category = EItemCategory::Equipment;

	// One piece per slot: a stack of boots can't be worn.
	MaxStackSize = 1;
}

#if WITH_EDITOR
bool UEquipmentDataAsset::CanEditChange(const FProperty* InProperty) const
{
	if (!Super::CanEditChange(InProperty))
	{
		return false;
	}

	// Fixed by the class: greyed out in the details panel.
	const FName PropertyName = InProperty ? InProperty->GetFName() : NAME_None;

	return PropertyName != GET_MEMBER_NAME_CHECKED(UEquipmentDataAsset, Category)
		&& PropertyName != GET_MEMBER_NAME_CHECKED(UEquipmentDataAsset, MaxStackSize);
}

EDataValidationResult UEquipmentDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!EquipmentSlot.IsValid())
	{
		Context.AddError(LOCTEXT("MissingSlot", "Equipment Slot is not set: the item can't be equipped."));
		Result = EDataValidationResult::Invalid;
	}

	if (Category != EItemCategory::Equipment || MaxStackSize != 1)
	{
		Context.AddError(LOCTEXT("WrongCategory", "Equipment must have Category Equipment and Max Stack Size 1."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
