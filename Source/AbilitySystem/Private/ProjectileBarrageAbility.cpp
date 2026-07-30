#include "ProjectileBarrageAbility.h"

#include "ArrowBase.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TargetingComponent.h"
#include "TimerManager.h"

void UProjectileBarrageAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	ClearBarrageTimer();
	BarrageImpactPoints.Reset();

	Super::OnAbilityEnded_Implementation(EndReason);
}

FVector UProjectileBarrageAbility::ResolveProjectileDirectionForIndex_Implementation(const int32 ProjectileIndex) const
{
	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	const FVector ForwardDirection = Character->GetActorForwardVector().GetSafeNormal();
	const FVector UpDirection = Character->GetActorUpVector().GetSafeNormal();

	FVector BaseLaunchDirection =
		ForwardDirection * ForwardLaunchStrength +
		UpDirection * UpwardLaunchStrength;

	BaseLaunchDirection = BaseLaunchDirection.GetSafeNormal();

	if (BaseLaunchDirection.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	if (LaunchSpreadAngle <= KINDA_SMALL_NUMBER)
	{
		return BaseLaunchDirection;
	}

	const int32 ProjectileCount = FMath::Max(ProjectileHandSocketNames.Num(), 1);

	if (ProjectileCount <= 1)
	{
		return BaseLaunchDirection;
	}

	const float AngleStep = 360.0f / static_cast<float>(ProjectileCount);
	const float YawAngle = AngleStep * static_cast<float>(ProjectileIndex);

	const FVector SpreadAxis = ForwardDirection
		.RotateAngleAxis(YawAngle, BaseLaunchDirection)
		.GetSafeNormal();

	return BaseLaunchDirection
		.RotateAngleAxis(LaunchSpreadAngle, SpreadAxis)
		.GetSafeNormal();
}

bool UProjectileBarrageAbility::ShouldUseCurrentTarget_Implementation() const
{
	return false;
}

void UProjectileBarrageAbility::OnProjectileReleased_Implementation(const float Strength)
{
	Super::OnProjectileReleased_Implementation(Strength);

	ClearBarrageTimer();
	BarrageImpactPoints.Reset();

	const int32 ProjectileCount = GetReleasedProjectileCount();

	if (ProjectileCount <= 0)
	{
		return;
	}

	const FVector TargetCenter = ResolveBarrageTargetCenter();

	BarrageImpactPoints.Reserve(ProjectileCount);

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		AArrowBase* Arrow = GetReleasedProjectile(Index);

		if (IsValid(Arrow) && BarrageFlightLifespan > 0.0f)
		{
			Arrow->SetRemainingFlightTime(BarrageFlightLifespan);
		}

		BarrageImpactPoints.Add(
			ResolveBarrageImpactPoint(
				Index,
				ProjectileCount,
				TargetCenter
			)
		);
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	if (RedirectDelay <= 0.0f)
	{
		RedirectReleasedProjectiles();
		return;
	}

	World->GetTimerManager().SetTimer(
		RedirectTimerHandle,
		this,
		&UProjectileBarrageAbility::RedirectReleasedProjectiles,
		RedirectDelay,
		false
	);
}

FVector UProjectileBarrageAbility::ResolveBarrageTargetCenter_Implementation() const
{
	const UTargetingComponent* MyTargetingComponent = GetTargetingComponent();

	if (IsValid(MyTargetingComponent) && MyTargetingComponent->HasTarget())
	{
		return MyTargetingComponent->GetCurrentTargetAimLocation();
	}

	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	const FVector ForwardDirection = Character->GetActorForwardVector().GetSafeNormal();

	return Character->GetActorLocation() +
		ForwardDirection * DefaultTargetDistance;
}

FVector UProjectileBarrageAbility::ResolveBarrageImpactPoint_Implementation(const int32 ProjectileIndex, const int32 ProjectileCount, const FVector& TargetCenter) const
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return TargetCenter;
	}

	FVector CandidatePoint = TargetCenter;

	if (ProjectileCount > 1 && ImpactRadius > KINDA_SMALL_NUMBER)
	{
		const float GoldenAngle = 137.507764f;
		const float NormalizedIndex = static_cast<float>(ProjectileIndex + 1) /
			static_cast<float>(ProjectileCount);

		const float Radius = FMath::Sqrt(NormalizedIndex) * ImpactRadius;
		const float AngleDegrees = GoldenAngle * static_cast<float>(ProjectileIndex);

		const FVector OffsetDirection = FVector::ForwardVector.RotateAngleAxis(
			AngleDegrees,
			FVector::UpVector
		);

		CandidatePoint += OffsetDirection * Radius;
	}

	const FVector TraceStart = CandidatePoint + FVector::UpVector * GroundTraceHeight;
	const FVector TraceEnd = CandidatePoint - FVector::UpVector * GroundTraceDepth;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwningCharacter());

	for (int32 Index = 0; Index < GetReleasedProjectileCount(); ++Index)
	{
		if (AArrowBase* Arrow = GetReleasedProjectile(Index))
		{
			QueryParams.AddIgnoredActor(Arrow);
		}
	}

	FHitResult HitResult;

	if (World->LineTraceSingleByChannel(
		HitResult,
		TraceStart,
		TraceEnd,
		GroundTraceChannel,
		QueryParams))
	{
		return HitResult.ImpactPoint;
	}

	return CandidatePoint;
}

void UProjectileBarrageAbility::RedirectReleasedProjectiles()
{
	ClearBarrageTimer();

	const int32 ProjectileCount = GetReleasedProjectileCount();

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		AArrowBase* Arrow = GetReleasedProjectile(Index);

		if (!IsValid(Arrow) ||
			!Arrow->IsInFlight() ||
			!BarrageImpactPoints.IsValidIndex(Index))
		{
			continue;
		}

		const FVector Direction = (
			BarrageImpactPoints[Index] -
			Arrow->GetActorLocation()
		).GetSafeNormal();

		if (!Direction.IsNearlyZero())
		{
			Arrow->Redirect(Direction);
		}
	}
}

void UProjectileBarrageAbility::ClearBarrageTimer()
{
	UWorld* World = GetWorld();

	if (IsValid(World))
	{
		World->GetTimerManager().ClearTimer(RedirectTimerHandle);
	}
}