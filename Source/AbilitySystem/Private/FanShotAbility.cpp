#include "FanShotAbility.h"

#include "BowBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"


void UFanShotAbility::ActivateAbility_Implementation()
{
	const float BaseSpread = SpreadMode == EFanSpreadMode::TotalAngle
		? TotalSpreadAngle
		: AngleBetweenArrows;

	ResolvedSpreadAngle = FMath::Max(GetModifiedFloat(AbilitySystemTags::Stat_Spread, BaseSpread), 0.0f);

	Super::ActivateAbility_Implementation();
}
bool UFanShotAbility::ShouldUseCurrentTarget_Implementation() const
{
	return false;
}

FVector UFanShotAbility::ResolveFanCenterDirection_Implementation() const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	// Prefer where the player is aiming (control rotation), fall back to facing.
	if (const AController* Controller = Character->GetController())
	{
		const FVector AimForward = Controller->GetControlRotation().Vector();
		FVector Flat(AimForward.X, AimForward.Y, 0.0f);
		if (!Flat.IsNearlyZero())
		{
			return Flat.GetSafeNormal();
		}
	}

	return Character->GetActorForwardVector().GetSafeNormal();
}

float UFanShotAbility::ComputeYawOffsetForIndex(const int32 ProjectileIndex, const int32 ProjectileCount) const
{
	if (ProjectileCount <= 1)
	{
		return 0.0f;
	}

	// Symmetric center: indices map to [-(N-1)/2 .. +(N-1)/2] steps.
	const float CenteredStep = static_cast<float>(ProjectileIndex) - (static_cast<float>(ProjectileCount - 1) * 0.5f);

	switch (SpreadMode)
	{
	case EFanSpreadMode::AnglePerArrow:
		return CenteredStep * ResolvedSpreadAngle;

	case EFanSpreadMode::TotalAngle:
	default:
		{
			const float GapAngle = ResolvedSpreadAngle / static_cast<float>(ProjectileCount - 1);
			return CenteredStep * GapAngle;
		}
	}
}

FVector UFanShotAbility::ResolveProjectileDirectionForIndex_Implementation(const int32 ProjectileIndex) const
{
	const FVector Center = ResolveFanCenterDirection();
	if (Center.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const int32 Count = IsValid(GetBow()) ? GetBow()->GetPreparedArrowCount() : 0;
	
	if (Count <= 1)
	{
		return Center;
	}

	const float YawOffset = ComputeYawOffsetForIndex(ProjectileIndex, Count);

	// Horizontal fan: rotate the center direction around world up.
	return Center.RotateAngleAxis(YawOffset, FVector::UpVector).GetSafeNormal();
}