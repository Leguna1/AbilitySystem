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

	// Seed the intent stability trackers so the first refresh doesn't read a
	// bogus swing against a default-constructed direction.
	PreviousCameraForward = GetTargetingForwardVector().GetSafeNormal();
	PreviousMoveDir = OwningCharacter->GetActorForwardVector();

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

	// Retention: keep the held target while it stays legal (range + LOS).
	// Angle is deliberately NOT re-checked here, so turning the camera or
	// running away from the target doesn't drop the lock on its own.
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

	// Accumulate intent every refresh (not only when we lack a target), then
	// let a challenger take over only if its intent beats the held target plus
	// the stickiness bonus. This is what produces a natural, deliberate swap.
	TArray<AActor*> Candidates;
	FindTargetCandidates(Candidates);

	UpdateIntent(Candidates, RefreshInterval);

	AActor* Challenger = SelectBestTarget(Candidates);

	if (!IsValid(CurrentTarget))
	{
		SetTarget(Challenger);
	}
	else if (IsValid(Challenger) && Challenger != CurrentTarget)
	{
		const float ChallengerIntent = GetAccumulatedIntent(Challenger);
		const float HeldIntent = GetAccumulatedIntent(CurrentTarget) + TargetStickinessBonus;

		if (ChallengerIntent > HeldIntent)
		{
			SetTarget(Challenger);
		}
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

void UTargetingComponent::UpdateIntent(const TArray<AActor*>& Candidates, float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector Origin = GetTargetingOrigin();

	// --- Primary axis: camera framing. Runs even while stationary. ---
	const FVector CameraForward = GetTargetingForwardVector().GetSafeNormal();

	const float CameraSwingDeg = FMath::RadiansToDegrees(FMath::Acos(
		FMath::Clamp(FVector::DotProduct(CameraForward, PreviousCameraForward), -1.0f, 1.0f)
	));
	const float CameraAngularSpeed = CameraSwingDeg / DeltaTime;
	const float CameraSettle = 1.0f - FMath::Clamp(CameraAngularSpeed / MaxCameraSweepSpeed, 0.0f, 1.0f);

	PreviousCameraForward = CameraForward;

	// --- Secondary axis: movement engagement. Only while actually moving. ---
	const FVector Velocity = IsValid(OwningCharacter)
		? OwningCharacter->GetVelocity()
		: FVector::ZeroVector;

	const FVector FlatVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float Speed = FlatVelocity.Size();
	const bool bMoving = Speed >= MinIntentSpeed;

	FVector MoveDir = FVector::ZeroVector;
	float CommitFactor = 0.0f;

	if (bMoving)
	{
		MoveDir = FlatVelocity / Speed;

		const float HeadingSwingDeg = FMath::RadiansToDegrees(FMath::Acos(
			FMath::Clamp(FVector::DotProduct(MoveDir, PreviousMoveDir), -1.0f, 1.0f)
		));
		const float HeadingAngularSpeed = HeadingSwingDeg / DeltaTime;
		CommitFactor = 1.0f - FMath::Clamp(HeadingAngularSpeed / MaxJinkAngularSpeed, 0.0f, 1.0f);

		PreviousMoveDir = MoveDir;
	}

	// --- Decay all records; drop dead or negligible ones. ---
	const float DecayMultiplier = FMath::Pow(IntentDecayRate, DeltaTime);

	for (int32 Index = IntentRecords.Num() - 1; Index >= 0; --Index)
	{
		IntentRecords[Index].IntentScore *= DecayMultiplier;

		if (!IntentRecords[Index].Target.IsValid() ||
			IntentRecords[Index].IntentScore < KINDA_SMALL_NUMBER)
		{
			IntentRecords.RemoveAtSwap(Index);
		}
	}

	// --- Accumulate this tick's intent per candidate. ---
	const float IntentCosine = FMath::Cos(FMath::DegreesToRadians(IntentConeAngle * 0.5f));

	for (AActor* Candidate : Candidates)
	{
		if (!IsValid(Candidate))
		{
			continue;
		}

		const FVector DirToCandidate = (ResolveTargetAimLocation(Candidate) - Origin).GetSafeNormal();
		if (DirToCandidate.IsNearlyZero())
		{
			continue;
		}

		float IntentGain = 0.0f;

		// Framing (primary): centeredness in the camera cone, gated by settle.
		const float CamDot = FVector::DotProduct(CameraForward, DirToCandidate);
		if (CamDot >= IntentCosine)
		{
			const float Denominator = 1.0f - IntentCosine;
			const float Centeredness = Denominator > KINDA_SMALL_NUMBER
				? (CamDot - IntentCosine) / Denominator
				: 1.0f;

			IntentGain += Centeredness * CameraSettle;
		}

		// Engagement (secondary): closing on or strafing around this candidate.
		if (bMoving)
		{
			const FVector FlatDir = FVector(DirToCandidate.X, DirToCandidate.Y, 0.0f).GetSafeNormal();
			if (!FlatDir.IsNearlyZero())
			{
				const float RadialDot = FVector::DotProduct(MoveDir, FlatDir); // +closing / -retreating

				const float Engagement =
					FMath::Clamp(RadialDot, 0.0f, 1.0f) +            // moving toward it
					(1.0f - FMath::Abs(RadialDot)) * 0.5f;           // orbiting / strafing it

				IntentGain += Engagement * CommitFactor * EngagementWeight;
			}
		}

		if (IntentGain <= 0.0f)
		{
			continue;
		}

		FTargetIntentRecord* Record = FindIntentRecord(Candidate);
		if (Record == nullptr)
		{
			Record = &IntentRecords.AddDefaulted_GetRef();
			Record->Target = Candidate;
		}

		Record->IntentScore = FMath::Min(Record->IntentScore + IntentGain * DeltaTime, MaxIntentScore);
	}
}

float UTargetingComponent::GetAccumulatedIntent(const AActor* Target) const
{
	for (const FTargetIntentRecord& Record : IntentRecords)
	{
		if (Record.Target.Get() == Target)
		{
			return Record.IntentScore;
		}
	}

	return 0.0f;
}

FTargetIntentRecord* UTargetingComponent::FindIntentRecord(const AActor* Target)
{
	for (FTargetIntentRecord& Record : IntentRecords)
	{
		if (Record.Target.Get() == Target)
		{
			return &Record;
		}
	}

	return nullptr;
}

AActor* UTargetingComponent::SelectBestTarget_Implementation(const TArray<AActor*>& Candidates) const
{
	AActor* BestTarget = nullptr;
	float BestIntent = KINDA_SMALL_NUMBER;

	for (AActor* Candidate : Candidates)
	{
		if (!IsValidTarget(Candidate))
		{
			continue;
		}

		const float Intent = GetAccumulatedIntent(Candidate);

		if (Intent > BestIntent)
		{
			BestIntent = Intent;
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

	// Free-look camera direction is the primary "look intent" axis. Under
	// orient-to-movement the control rotation is decoupled from character
	// facing, which is exactly what we want to read here.
	if (const AController* Controller = OwningCharacter->GetController())
	{
		return Controller->GetControlRotation().Vector();
	}

	return OwningCharacter->GetActorForwardVector();
}