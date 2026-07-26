#include "TargetingComponent.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TargetableInterface.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UTargetingComponent::UTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTargetingComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningCharacter = Cast<ACharacter>(GetOwner());

	if (!IsValid(OwningCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("UTargetingComponent requires an ACharacter owner."));
		return;
	}

	if (ReleaseRange < AcquisitionRange)
	{
		ReleaseRange = AcquisitionRange;
	}

	if (bAutoRefresh)
	{
		GetWorld()->GetTimerManager().SetTimer(
			RefreshTimerHandle,
			this,
			&UTargetingComponent::RefreshTarget,
			RefreshInterval,
			true,
			0.0f
		);
	}
}

void UTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(GetWorld()))
	{
		GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
	}

	ClearTarget();

	Super::EndPlay(EndPlayReason);
}

void UTargetingComponent::RefreshTarget()
{
	if (!IsValid(OwningCharacter))
	{
		ClearTarget();
		return;
	}

	if (IsValid(CurrentTarget))
	{
		const float DistanceSquared = FVector::DistSquared(
			OwningCharacter->GetActorLocation(),
			CurrentTarget->GetActorLocation()
		);

		const bool bStillValid =
			IsValidTarget(CurrentTarget) &&
			DistanceSquared <= FMath::Square(ReleaseRange) &&
			(!bRequireLineOfSight || HasLineOfSightTo(CurrentTarget));

		if (!bStillValid)
		{
			ClearTarget();
		}
	}

	if (!IsValid(CurrentTarget))
	{
		TArray<AActor*> Candidates;
		FindTargetCandidates(Candidates);

		SetTarget(SelectBestTarget(Candidates));
	}

	if (bDrawTargetDebugSphere && IsValid(CurrentTarget))
	{
		DrawDebugSphere(
			GetWorld(),
			GetCurrentTargetAimLocation(),
			TargetDebugSphereRadius,
			TargetDebugSphereSegments,
			FColor::Green,
			false,
			RefreshInterval * 1.25f,
			0,
			1.5f
		);
	}
}

void UTargetingComponent::ClearTarget()
{
	SetTarget(nullptr);
}

bool UTargetingComponent::SetTarget(AActor* NewTarget)
{
	if (NewTarget == CurrentTarget)
	{
		return IsValid(CurrentTarget);
	}

	if (IsValid(NewTarget) && !IsValidTarget(NewTarget))
	{
		return false;
	}

	AActor* PreviousTarget = CurrentTarget;
	CurrentTarget = NewTarget;

	OnTargetChanged.Broadcast(PreviousTarget, CurrentTarget);
	return IsValid(CurrentTarget);
}

bool UTargetingComponent::IsValidTarget(AActor* Candidate) const
{
	if (!IsValid(Candidate) ||
		Candidate == GetOwner() ||
		Candidate->IsActorBeingDestroyed() ||
		!Candidate->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
	{
		return false;
	}

	if (!ITargetableInterface::Execute_IsTargetable(Candidate))
	{
		return false;
	}

	return PassesCustomTargetFilter(Candidate);
}

FVector UTargetingComponent::GetCurrentTargetAimLocation() const
{
	return IsValid(CurrentTarget)
		? ResolveTargetAimLocation(CurrentTarget)
		: FVector::ZeroVector;
}

FVector UTargetingComponent::GetDirectionToCurrentTarget(const FVector& Origin) const
{
	if (!IsValid(CurrentTarget))
	{
		return FVector::ZeroVector;
	}

	return (GetCurrentTargetAimLocation() - Origin).GetSafeNormal();
}

AActor* UTargetingComponent::SelectBestTarget_Implementation(const TArray<AActor*>& Candidates) const
{
	AActor* BestTarget = nullptr;
	float BestScore = -TNumericLimits<float>::Max();

	for (AActor* Candidate : Candidates)
	{
		if (!IsValidTarget(Candidate))
		{
			continue;
		}

		const float Score = CalculateTargetScore(Candidate);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool UTargetingComponent::PassesCustomTargetFilter_Implementation(AActor* Candidate) const
{
	return true;
}

FVector UTargetingComponent::ResolveTargetAimLocation_Implementation(AActor* Target) const
{
	if (!IsValid(Target))
	{
		return FVector::ZeroVector;
	}

	if (Target->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass()))
	{
		return ITargetableInterface::Execute_GetTargetAimLocation(Target);
	}

	return Target->GetActorLocation();
}

void UTargetingComponent::FindTargetCandidates(TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();

	if (!IsValid(OwningCharacter) ||
		AcquisitionRange <= 0.0f ||
		TargetObjectTypes.IsEmpty())
	{
		return;
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(OwningCharacter);

	TArray<AActor*> OverlappingActors;

	UKismetSystemLibrary::SphereOverlapActors(
		this,
		OwningCharacter->GetActorLocation(),
		AcquisitionRange,
		TargetObjectTypes,
		TargetClassFilter,
		ActorsToIgnore,
		OverlappingActors
	);

	for (AActor* Candidate : OverlappingActors)
	{
		if (!IsValidTarget(Candidate) ||
			!IsInsideAcquisitionAngle(Candidate) ||
			(bRequireLineOfSight && !HasLineOfSightTo(Candidate)))
		{
			continue;
		}

		OutCandidates.Add(Candidate);
	}
}

bool UTargetingComponent::IsInsideAcquisitionAngle(AActor* Candidate) const
{
	if (!IsValid(Candidate) || AcquisitionAngle >= 360.0f)
	{
		return IsValid(Candidate);
	}

	const FVector Origin = GetTargetingOrigin();
	const FVector Forward = GetTargetingForwardVector().GetSafeNormal();
	const FVector DirectionToTarget =
		(ResolveTargetAimLocation(Candidate) - Origin).GetSafeNormal();

	if (Forward.IsNearlyZero() || DirectionToTarget.IsNearlyZero())
	{
		return false;
	}

	const float MinimumDot = FMath::Cos(
		FMath::DegreesToRadians(AcquisitionAngle * 0.5f)
	);

	return FVector::DotProduct(Forward, DirectionToTarget) >= MinimumDot;
}

bool UTargetingComponent::HasLineOfSightTo(AActor* Candidate) const
{
	if (!IsValid(Candidate) || !IsValid(GetWorld()))
	{
		return false;
	}

	const FVector Start = GetTargetingOrigin();
	const FVector End = ResolveTargetAimLocation(Candidate);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TargetingLineOfSight), false);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.AddIgnoredActor(Candidate);

	FHitResult HitResult;

	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		LineOfSightTraceChannel,
		QueryParams
	);

	return !bBlocked;
}

float UTargetingComponent::CalculateTargetScore(AActor* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return -TNumericLimits<float>::Max();
	}

	const FVector Origin = GetTargetingOrigin();
	const FVector AimLocation = ResolveTargetAimLocation(Candidate);
	const FVector DirectionToTarget = (AimLocation - Origin).GetSafeNormal();
	const FVector Forward = GetTargetingForwardVector().GetSafeNormal();

	const float Distance = FVector::Distance(Origin, AimLocation);
	const float NormalizedDistance = AcquisitionRange > 0.0f
		? FMath::Clamp(Distance / AcquisitionRange, 0.0f, 1.0f)
		: 1.0f;

	const float DistanceScore = 1.0f - NormalizedDistance;

	const float Alignment = FMath::Clamp(
		FVector::DotProduct(Forward, DirectionToTarget),
		-1.0f,
		1.0f
	);

	const float AngleScore = (Alignment + 1.0f) * 0.5f;

	return AngleScore * AngleScoreWeight +
		DistanceScore * DistanceScoreWeight;
}

FVector UTargetingComponent::GetTargetingOrigin() const
{
	if (!IsValid(OwningCharacter))
	{
		return FVector::ZeroVector;
	}

	FVector ViewLocation;
	FRotator ViewRotation;

	if (const AController* Controller = OwningCharacter->GetController())
	{
		Controller->GetActorEyesViewPoint(ViewLocation, ViewRotation);
		return ViewLocation;
	}

	OwningCharacter->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	return ViewLocation;
}

FVector UTargetingComponent::GetTargetingForwardVector() const
{
	if (!IsValid(OwningCharacter))
	{
		return FVector::ForwardVector;
	}

	if (const AController* Controller = OwningCharacter->GetController())
	{
		return Controller->GetControlRotation().Vector();
	}

	return OwningCharacter->GetActorForwardVector();
}