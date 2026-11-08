#include "WeaponDataAsset.h"

#include "WeaponBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "WeaponDataAsset"

#if WITH_EDITOR
EDataValidationResult UWeaponDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!WeaponClass)
	{
		Context.AddError(LOCTEXT("MissingWeaponClass", "Weapon Class is not set: nothing spawns when it is equipped."));
		Result = EDataValidationResult::Invalid;
	}

	if (EquipSocketName.IsNone())
	{
		Context.AddWarning(LOCTEXT("MissingEquipSocket", "Equip Socket Name is not set: the drawn weapon sits at the wielder's root."));
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
