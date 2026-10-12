#include "ProjectileDataAsset.h"

#include "Engine/StaticMesh.h"
#include "NiagaraSystem.h"
#include "ProjectileBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UProjectileDataAsset::UProjectileDataAsset()
{
	ProjectileClass = AProjectileBase::StaticClass();
}

#if WITH_EDITOR
EDataValidationResult UProjectileDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!ProjectileClass)
	{
		Context.AddError(NSLOCTEXT("ProjectileData", "NoClass", "Projectile Class is empty, so nothing can be fired. Pick Projectile Base or a child of it."));
		Result = EDataValidationResult::Invalid;
	}

	if (!IsValid(Mesh) && !IsValid(OngoingEffect))
	{
		Context.AddWarning(NSLOCTEXT("ProjectileData", "Invisible", "No Mesh and no Ongoing Effect: the projectile will be invisible in flight."));
	}

	if (MaximumSpeed <= 0.0f)
	{
		Context.AddError(NSLOCTEXT("ProjectileData", "NoSpeed", "Maximum Speed is 0, so the projectile can't be fired."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
