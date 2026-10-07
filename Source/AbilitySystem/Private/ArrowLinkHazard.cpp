#include "ArrowLinkHazard.h"

#include "ArrowBase.h"
#include "CombatantComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

AArrowLinkHazard::AArrowLinkHazard()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AArrowLinkHazard::InitializeHazard(const FArrowLinkHazardParams& InParams)
{
	Params = InParams;

	GetWorldTimerManager().SetTimer(
		CollectionTimerHandle,
		this,
		&AArrowLinkHazard::HandleCollectionTimeout,
		FMath::Max(Params.CollectionTime, 0.1f),
		false
	);
}

void AArrowLinkHazard::RegisterProjectile(AArrowBase* Arrow)
{
	if (!IsValid(Arrow) || bEnded)
	{
		return;
	}

	TrackedArrows.Add(Arrow, Arrow->GetFlightSerial());
	Arrow->OnArrowHit.AddUniqueDynamic(this, &AArrowLinkHazard::HandleArrowHit);
}

void AArrowLinkHazard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCollecting();
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void AArrowLinkHazard::HandleArrowHit(AArrowBase* Arrow, AActor* HitActor, float Damage, const FHitResult& Hit)
{
	const uint32* RegisteredSerial = TrackedArrows.Find(Arrow);
	const bool bSameFlight = RegisteredSerial && IsValid(Arrow) && *RegisteredSerial == Arrow->GetFlightSerial();

	// First hit only: a piercing arrow reports every target it passes through.
	UnbindArrow(Arrow);

	// A pooled arrow may have been fired again by something else since it was registered.
	if (!bSameFlight)
	{
		return;
	}

	const FVector Landing = Hit.ImpactPoint.IsNearlyZero()
		? Arrow->GetActorLocation()
		: FVector(Hit.ImpactPoint);

	AddLinkPoint(Landing);
}

void AArrowLinkHazard::AddLinkPoint(const FVector& LandingLocation)
{
	UWorld* World = GetWorld();

	if (bEnded || !IsValid(World))
	{
		return;
	}

	// Lines lie on the ground under the arrows, including arrows that landed in a body.
	FVector Point = LandingLocation;
	FHitResult GroundHit;
	FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(ArrowLinkGround), false, this);

	if (World->LineTraceSingleByObjectType(
		GroundHit,
		LandingLocation + FVector::UpVector * 50.0f,
		LandingLocation - FVector::UpVector * GroundTraceDistance,
		FCollisionObjectQueryParams(ECC_WorldStatic),
		GroundParams))
	{
		Point = FVector(GroundHit.ImpactPoint);
	}

	Point.Z += LineHeight;

	// Every existing point, closest first.
	TArray<int32> Neighbours;
	Neighbours.Reserve(Points.Num());

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		Neighbours.Add(Index);
	}

	Neighbours.Sort([this, &Point](const int32 A, const int32 B)
	{
		return FVector::DistSquared(Points[A], Point) < FVector::DistSquared(Points[B], Point);
	});

	Points.Add(Point);

	if (PointEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			PointEffect, GetRootComponent(), NAME_None, Point, FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition, true
		);
	}

	OnLinkPointAdded(Point);

	int32 LinkCount = 0;

	for (int32 Order = 0; Order < Neighbours.Num() && LinkCount < MaxLinksPerPoint; ++Order)
	{
		const FVector& Neighbour = Points[Neighbours[Order]];
		const float Distance = FVector::Dist(Neighbour, Point);

		if (MaxLineLength > 0.0f && Distance > MaxLineLength)
		{
			break;
		}

		// The nearest point joins this landing to the web; others only link within LinkDistance.
		const bool bJoinsWeb = Order == 0 && bAlwaysLinkNearest;

		if (!bJoinsWeb && Distance > Params.LinkDistance)
		{
			break;
		}

		CreateSegment(Neighbour, Point);
		++LinkCount;
	}

	// The storm starts with the first landing.
	if (Points.Num() == 1)
	{
		FTimerManager& TimerManager = GetWorldTimerManager();
		TimerManager.SetTimer(LifetimeTimerHandle, this, &AArrowLinkHazard::EndHazard, FMath::Max(Params.Duration, 0.1f), false);
		TimerManager.SetTimer(PulseTimerHandle, this, &AArrowLinkHazard::Pulse, PulseInterval, true);
	}
}

void AArrowLinkHazard::CreateSegment(const FVector& Start, const FVector& End)
{
	Segments.Add({ Start, End });

	if (LineEffect)
	{
		if (UNiagaraComponent* Beam = UNiagaraFunctionLibrary::SpawnSystemAttached(
			LineEffect, GetRootComponent(), NAME_None, Start, FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition, true))
		{
			Beam->SetVariableVec3(LineStartParameter, Start);
			Beam->SetVariableVec3(LineEndParameter, End);
		}
	}

	OnLinkCreated(Start, End);
}

void AArrowLinkHazard::Pulse()
{
	UWorld* World = GetWorld();

	if (bEnded || !IsValid(World) || Segments.IsEmpty())
	{
		return;
	}

	AActor* InstigatorActor = GetInstigator();

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArrowLinkPulse), false, this);

	if (!bAffectsInstigator)
	{
		QueryParams.AddIgnoredActor(InstigatorActor);
	}

	TArray<AActor*> Touched;

	for (const FLinkSegment& Segment : Segments)
	{
		const FVector Direction = Segment.End - Segment.Start;
		const float Length = Direction.Size();

		if (Length <= UE_KINDA_SMALL_NUMBER)
		{
			continue;
		}

		// A capsule lying along the line.
		const FVector Center = (Segment.Start + Segment.End) * 0.5f;
		const FQuat Rotation = FRotationMatrix::MakeFromZ(Direction / Length).ToQuat();
		const float HalfHeight = Length * 0.5f + LineRadius;

		TArray<FOverlapResult> Overlaps;

		World->OverlapMultiByObjectType(
			Overlaps,
			Center,
			Rotation,
			FCollisionObjectQueryParams(AffectedObjectChannel),
			FCollisionShape::MakeCapsule(LineRadius, HalfHeight),
			QueryParams
		);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (AActor* Actor = Overlap.GetActor())
			{
				Touched.AddUnique(Actor);
			}
		}

		if (bDrawDebugLines)
		{
			DrawDebugCapsule(World, Center, HalfHeight, LineRadius, Rotation, FColor::Cyan, false, PulseInterval);
		}
	}

	for (AActor* Actor : Touched)
	{
		UCombatantComponent* Combatant = IsValid(Actor) ? Actor->FindComponentByClass<UCombatantComponent>() : nullptr;

		if (!IsValid(Combatant) || Combatant->IsDead())
		{
			continue;
		}

		// Same source as the archer's other statuses, so a re-touch refreshes instead of stacking.
		for (const FStatusApplication& Status : Params.Statuses)
		{
			Combatant->ApplyStatus(Status, InstigatorActor);
		}

		OnActorAffected(Actor);
	}
}

void AArrowLinkHazard::HandleCollectionTimeout()
{
	StopCollecting();

	// Nothing landed: no storm.
	if (Points.IsEmpty())
	{
		EndHazard();
	}
}

void AArrowLinkHazard::StopCollecting()
{
	TArray<TWeakObjectPtr<AArrowBase>> Arrows;
	TrackedArrows.GetKeys(Arrows);

	for (const TWeakObjectPtr<AArrowBase>& Arrow : Arrows)
	{
		UnbindArrow(Arrow.Get());
	}

	TrackedArrows.Reset();
}

void AArrowLinkHazard::UnbindArrow(AArrowBase* Arrow)
{
	if (IsValid(Arrow))
	{
		Arrow->OnArrowHit.RemoveDynamic(this, &AArrowLinkHazard::HandleArrowHit);
	}

	TrackedArrows.Remove(Arrow);
}

void AArrowLinkHazard::EndHazard()
{
	if (bEnded)
	{
		return;
	}

	bEnded = true;
	OnHazardEnded();
	Destroy();
}
#if WITH_EDITOR
#include "AbilityValidation.h"

EDataValidationResult AArrowLinkHazard::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Statuses.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("ArrowLinkHazard", "NoStatuses", "No Statuses: the lines are purely visual."));
	}

	return CombineDataValidationResults(Result,
		AbilityValidation::ValidateStatusSpecs(Statuses, TEXT("Statuses"), Context));
}
#endif