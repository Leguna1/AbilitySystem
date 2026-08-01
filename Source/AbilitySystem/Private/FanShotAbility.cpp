#include "FanShotAbility.h"

#include "BowComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

bool UFanShotAbility::ShouldUseCurrentTarget_Implementation() const
{
	return false;
}

void UFanShotAbility::OnProjectileReleased_Implementation(const float Strength)
{
	Super::OnProjectileReleased_Implementation(Strength);

	// The fan leaving the bow is the commit point for this single-burst ability.
	if (!IsCommitted())
	{
		RequestCommit();
	}
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
		return CenteredStep * AngleBetweenArrows;

	case EFanSpreadMode::TotalAngle:
	default:
	{
		const float GapAngle = TotalSpreadAngle / static_cast<float>(ProjectileCount - 1);
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

	const int32 Count = IsValid(GetBowComponent())
		? GetBowComponent()->GetPreparedArrowCount()
		: 0;

	if (Count <= 1)
	{
		return Center;
	}

	const float YawOffset = ComputeYawOffsetForIndex(ProjectileIndex, Count);

	// Horizontal fan: rotate the center direction around world up.
	return Center.RotateAngleAxis(YawOffset, FVector::UpVector).GetSafeNormal();
}