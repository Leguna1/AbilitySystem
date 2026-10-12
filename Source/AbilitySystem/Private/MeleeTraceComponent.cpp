#include "MeleeTraceComponent.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GenericTeamAgentInterface.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UMeleeTraceComponent::UMeleeTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// After animation has updated, so sockets are where this frame's pose put them.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UMeleeTraceComponent::BeginPlay()
{
	Super::BeginPlay();

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	Mesh = IsValid(Character) ? Character->GetMesh() : GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
}

void UMeleeTraceComponent::BeginTrace(const TArray<FName>& ShapeNames, UAnimSequenceBase* SourceAnimation)
{
	if (!IsValid(Mesh))
	{
		return;
	}

	const bool bNewWindow = ActiveShapes.IsEmpty();

	if (bNewWindow)
	{
		HitActorsThisWindow.Reset();
		WindowSource = SourceAnimation;
	}

	for (const FName& ShapeName : ShapeNames)
	{
		const int32 ShapeIndex = Shapes.IndexOfByPredicate([ShapeName](const FMeleeTraceShape& Shape) { return Shape.ShapeName == ShapeName; });

		if (ShapeIndex == INDEX_NONE)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Melee Trace shape '%s' is not defined on %s."),
				*GetNameSafe(SourceAnimation), *ShapeName.ToString(), *GetNameSafe(GetOwner()));
			continue;
		}

		if (ActiveShapes.ContainsByPredicate([ShapeIndex](const FActiveShape& Active) { return Active.ShapeIndex == ShapeIndex; }))
		{
			continue;
		}

		FActiveShape& Active = ActiveShapes.AddDefaulted_GetRef();
		Active.ShapeIndex = ShapeIndex;
		Active.Source = SourceAnimation;

		// The first sweep starts where the part is now, so a target already touching it is hit.
		SamplePoints(Shapes[ShapeIndex], Active.PreviousPoints);
	}

	if (ActiveShapes.IsEmpty())
	{
		return;
	}

	SetComponentTickEnabled(true);

	if (bNewWindow)
	{
		OnTraceStarted.Broadcast(SourceAnimation);
	}
}

void UMeleeTraceComponent::EndTrace(const TArray<FName>& ShapeNames)
{
	if (ActiveShapes.IsEmpty())
	{
		return;
	}

	ActiveShapes.RemoveAll([this, &ShapeNames](const FActiveShape& Active)
	{
		return ShapeNames.Contains(Shapes[Active.ShapeIndex].ShapeName);
	});

	if (ActiveShapes.IsEmpty())
	{
		SetComponentTickEnabled(false);
		OnTraceEnded.Broadcast(WindowSource.Get());
	}
}

void UMeleeTraceComponent::EndAllTraces()
{
	if (ActiveShapes.IsEmpty())
	{
		return;
	}

	ActiveShapes.Reset();
	SetComponentTickEnabled(false);
	OnTraceEnded.Broadcast(WindowSource.Get());
}

void UMeleeTraceComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Sweep everything first, broadcast after: a listener may end the traces or kill the target.
	TArray<FPendingHit> Hits;

	for (FActiveShape& Active : ActiveShapes)
	{
		TraceShape(Active, Hits);
	}

	for (const FPendingHit& Pending : Hits)
	{
		if (AActor* Actor = Pending.Actor.Get())
		{
			OnMeleeHit.Broadcast(Actor, Pending.Hit, Pending.ShapeName, Pending.Source.Get());
		}
	}
}

void UMeleeTraceComponent::TraceShape(FActiveShape& Active, TArray<FPendingHit>& OutHits)
{
	UWorld* World = GetWorld();
	const FMeleeTraceShape& Shape = Shapes[Active.ShapeIndex];

	TArray<FVector> Current;
	SamplePoints(Shape, Current);

	if (Active.PreviousPoints.Num() != Current.Num())
	{
		Active.PreviousPoints = Current;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeTrace), false, GetOwner());
	const FCollisionObjectQueryParams ObjectParams(TraceObjectChannel);
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(Shape.Radius);

	for (int32 Index = 0; Index < Current.Num(); ++Index)
	{
		TArray<FHitResult> SweepHits;
		World->SweepMultiByObjectType(SweepHits, Active.PreviousPoints[Index], Current[Index], FQuat::Identity, ObjectParams, Sphere, QueryParams);

		for (const FHitResult& Hit : SweepHits)
		{
			AActor* Actor = Hit.GetActor();

			if (!ShouldHit(Actor))
			{
				continue;
			}

			HitActorsThisWindow.Add(Actor);
			OutHits.Add({ Actor, Hit, Shape.ShapeName, Active.Source });
		}

		if (bDrawDebug)
		{
			DrawDebugSphere(World, Current[Index], Shape.Radius, 8, FColor::Red, false, 0.2f);
			DrawDebugLine(World, Active.PreviousPoints[Index], Current[Index], FColor::Orange, false, 0.2f);
		}
	}

	Active.PreviousPoints = MoveTemp(Current);
}

void UMeleeTraceComponent::SamplePoints(const FMeleeTraceShape& Shape, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();

	const FVector Start = Mesh->GetSocketLocation(Shape.StartSocket);

	if (Shape.EndSocket.IsNone())
	{
		OutPoints.Add(Start);
		return;
	}

	// Spheres along the part, close enough to overlap, so the whole limb is covered.
	const FVector End = Mesh->GetSocketLocation(Shape.EndSocket);
	const int32 Segments = FMath::Clamp(FMath::CeilToInt(FVector::Dist(Start, End) / FMath::Max(Shape.Radius, 1.0f)), 1, 8);

	for (int32 Step = 0; Step <= Segments; ++Step)
	{
		OutPoints.Add(FMath::Lerp(Start, End, static_cast<float>(Step) / Segments));
	}
}

bool UMeleeTraceComponent::ShouldHit(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor == GetOwner() || HitActorsThisWindow.Contains(Actor))
	{
		return false;
	}

	return !bIgnoreFriendly || FGenericTeamId::GetAttitude(GetOwner(), Actor) != ETeamAttitude::Friendly;
}

#if WITH_EDITOR

EDataValidationResult UMeleeTraceComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FName> Seen;

	for (int32 Index = 0; Index < Shapes.Num(); ++Index)
	{
		const FMeleeTraceShape& Shape = Shapes[Index];

		if (Shape.ShapeName.IsNone() || Shape.StartSocket.IsNone())
		{
			Context.AddError(FText::Format(NSLOCTEXT("MeleeTrace", "Incomplete", "Shapes [{0}] needs a Shape Name and a Start Socket."), Index));
			Result = EDataValidationResult::Invalid;
		}
		else if (Seen.Contains(Shape.ShapeName))
		{
			Context.AddError(FText::Format(NSLOCTEXT("MeleeTrace", "Duplicate", "Shape name {0} is used twice."), FText::FromName(Shape.ShapeName)));
			Result = EDataValidationResult::Invalid;
		}

		Seen.Add(Shape.ShapeName);
	}

	return Result;
}

#endif