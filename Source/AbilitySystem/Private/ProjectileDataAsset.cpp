#include "ProjectileDataAsset.h"

#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include "Engine/StaticMesh.h"
#include "FeedbackSet.h"
#include "FeedbackTags.h"
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

	const FFeedbackCue* FlightCue = IsValid(FeedbackSet) ? FeedbackSet->FindCue(FeedbackTags::Cue_Projectile_Flight) : nullptr;

	if (!IsValid(Mesh) && (FlightCue == nullptr || !FlightCue->Effect.HasEffect()))
	{
		Context.AddWarning(NSLOCTEXT("ProjectileData", "Invisible", "No Mesh and no Cue.Projectile.Flight effect in the Feedback Set: the projectile will be invisible in flight."));
	}

	if (!IsValid(FeedbackSet))
	{
		Context.AddWarning(NSLOCTEXT("ProjectileData", "NoFeedback", "No Feedback Set: hits play only what the project's default set defines."));
	}

	if (MaximumSpeed <= 0.0f)
	{
		Context.AddError(NSLOCTEXT("ProjectileData", "NoSpeed", "Maximum Speed is 0, so the projectile can't be fired."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
