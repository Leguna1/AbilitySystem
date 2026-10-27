// UIBarMaterial.cpp
#include "UIBarMaterial.h"

#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void FUIBarMaterial::Initialize(UImage* Image, const FUIBarMaterialSettings& InSettings)
{
	Settings = InSettings;
	Material = nullptr;

	if (!::IsValid(Image))
	{
		return;
	}

	if (::IsValid(Settings.Material))
	{
		Image->SetBrushFromMaterial(Settings.Material);
	}

	Material = Image->GetDynamicMaterial();
	Apply();
}

void FUIBarMaterial::SetPercent(const float NewPercent, const bool bInstant)
{
	const float Clamped = FMath::Clamp(NewPercent, 0.0f, 1.0f);

	if (bInstant || Settings.TrailDuration <= 0.0f || Clamped >= Trail)
	{
		// Healing, refills and first fills: no strip.
		Percent = Clamped;
		Trail = Clamped;
		bTrailMoving = false;
	}
	else
	{
		// A drop: the strip stays where the bar was, then slides down. Further drops restart its delay.
		TrailFrom = Trail;
		Percent = Clamped;
		TrailElapsed = 0.0f;
		bTrailMoving = true;
	}

	Apply();
}

bool FUIBarMaterial::Tick(const float DeltaTime)
{
	if (!bTrailMoving)
	{
		return false;
	}

	TrailElapsed += DeltaTime;

	const float MoveTime = TrailElapsed - Settings.TrailDelay;

	if (MoveTime > 0.0f)
	{
		const float Alpha = FMath::Clamp(MoveTime / FMath::Max(Settings.TrailDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		Trail = FMath::InterpEaseOut(TrailFrom, Percent, Alpha, 2.0f);

		if (Alpha >= 1.0f)
		{
			Trail = Percent;
			bTrailMoving = false;
		}

		Apply();
	}

	return bTrailMoving;
}

void FUIBarMaterial::SetScalar(const FName Parameter, const float Value) const
{
	if (::IsValid(Material) && !Parameter.IsNone())
	{
		Material->SetScalarParameterValue(Parameter, Value);
	}
}

void FUIBarMaterial::Apply() const
{
	if (!::IsValid(Material))
	{
		return;
	}

	Material->SetScalarParameterValue(Settings.PercentParameter, Percent);
	Material->SetScalarParameterValue(Settings.TrailParameter, Trail);

	const bool bLow = Settings.LowPulseThreshold > 0.0f && Percent < Settings.LowPulseThreshold;
	Material->SetScalarParameterValue(Settings.LowPulseParameter, bLow ? 1.0f : 0.0f);
}
