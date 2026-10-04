#include "ProjectileBarrageAbility.h"

#include "ArrowBase.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TargetingComponent.h"
#include "TimerManager.h"
#include "AbilitySystemTags.h"
#include "CombatantComponent.h"
#include "Engine/OverlapResult.h"


void UProjectileBarrageAbility::ActivateAbility_Implementation()
{
	ResolvedImpactRadius = FMath::Max(GetModifiedFloat(AbilitySystemTags::Stat_Radius, ImpactRadius), 0.0f);
	ResolvedSeekChance = FMath::Clamp(GetModifiedFloat(AbilitySystemTags::Stat_SeekChance, BaseSeekChance), 0.0f, 1.0f);
	
	Super::ActivateAbility_Implementation();
}
UProjectileBarrageAbility::UProjectileBarrageAbility()
{
	CostTrigger = EAbilityCostTrigger::OnAnimationEvent;
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

	const int32 ProjectileCount = FMath::Max(GetProjectileCount(), 1);

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

	const int32 ProjectileCount = GetReleasedProjectileCount();

	if (ProjectileCount <= 0)
	{
		return;
	}

	const FVector TargetCenter = ResolveBarrageTargetCenter();

	// Gathered once per release; stays empty when this volley doesn't seek.
	TArray<AActor*> SeekCandidates;

	if (ResolvedSeekChance > 0.0f)
	{
		FindSeekTargets(TargetCenter, SeekCandidates);
	}

	TArray<int32> AssignedCounts;
	AssignedCounts.Init(0, SeekCandidates.Num());

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		AArrowBase* Arrow = GetReleasedProjectile(Index);

		if (!IsValid(Arrow))
		{
			continue;
		}

		// Safety net for arrows that never reach their point.
		if (BarrageFlightLifespan > 0.0f)
		{
			Arrow->SetRemainingFlightTime(BarrageFlightLifespan);
		}

		const float Stagger = BarrageImpactStagger > 0.0f
			? FMath::FRandRange(0.0f, BarrageImpactStagger)
			: 0.0f;

		const float Delay = RedirectDelay + Stagger;

		// Also the fallback for a seeking arrow whose target dies before it turns.
		const FVector LandingPoint = ResolveBarrageImpactPoint(Index, ProjectileCount, TargetCenter);

		AActor* SeekTarget = !SeekCandidates.IsEmpty() && FMath::FRand() < ResolvedSeekChance
			? PickSeekTarget(SeekCandidates, AssignedCounts)
			: nullptr;

		if (IsValid(SeekTarget))
		{
			Arrow->ScheduleRedirectToActor(SeekTarget, LandingPoint, Delay, true, SeekHomingAcceleration);
		}
		else
		{
			Arrow->ScheduleRedirect(LandingPoint, Delay, true);
		}
	}
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

	if (ProjectileCount > 1 && ResolvedImpactRadius > KINDA_SMALL_NUMBER)
	{
		const float GoldenAngle = 137.507764f;
		const float NormalizedIndex = static_cast<float>(ProjectileIndex + 1) /
			static_cast<float>(ProjectileCount);

		const float Radius = FMath::Sqrt(NormalizedIndex) * ResolvedImpactRadius;
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
void UProjectileBarrageAbility::FindSeekTargets(const FVector& Center, TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	UWorld* World = GetWorld();
	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(World) || SeekRadius <= 0.0f)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VolleySeek), false, Character);
	TArray<FOverlapResult> Overlaps;

	World->OverlapMultiByObjectType(
		Overlaps,
		Center,
		FQuat::Identity,
		FCollisionObjectQueryParams(SeekObjectChannel),
		FCollisionShape::MakeSphere(SeekRadius),
		QueryParams
	);

	const UTargetingComponent* Targeting = GetTargetingComponent();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();

		if (!IsValid(Candidate) || Candidate == Character || OutTargets.Contains(Candidate))
		{
			continue;
		}

		const UCombatantComponent* Combatant = Candidate->FindComponentByClass<UCombatantComponent>();

		if (!IsValid(Combatant) || Combatant->IsDead())
		{
			continue;
		}

		// Same rules as the player's targeting (targetable, custom filters) when available.
		if (IsValid(Targeting) && !Targeting->IsValidTarget(Candidate))
		{
			continue;
		}

		OutTargets.Add(Candidate);
	}

	const FGameplayTagContainer& PreferredTags = PreferredTargetStatusTags;

	auto IsPreferred = [&PreferredTags](const AActor& Candidate)
	{
		if (PreferredTags.IsEmpty())
		{
			return false;
		}

		const UCombatantComponent* Combatant = Candidate.FindComponentByClass<UCombatantComponent>();
		return IsValid(Combatant) && Combatant->GetStatusTags().HasAny(PreferredTags);
	};

	// Preferred (e.g. marked) first, then closest to the volley's center.
	OutTargets.Sort([&IsPreferred, &Center](const AActor& A, const AActor& B)
	{
		const bool bAPreferred = IsPreferred(A);
		const bool bBPreferred = IsPreferred(B);

		if (bAPreferred != bBPreferred)
		{
			return bAPreferred;
		}

		return FVector::DistSquared(A.GetActorLocation(), Center) < FVector::DistSquared(B.GetActorLocation(), Center);
	});
	
}

AActor* UProjectileBarrageAbility::PickSeekTarget(const TArray<AActor*>& Candidates, TArray<int32>& AssignedCounts)
{
	int32 BestIndex = INDEX_NONE;

	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (BestIndex == INDEX_NONE || AssignedCounts[Index] < AssignedCounts[BestIndex])
		{
			BestIndex = Index;
		}
	}

	if (BestIndex == INDEX_NONE)
	{
		return nullptr;
	}

	++AssignedCounts[BestIndex];
	return Candidates[BestIndex];
}