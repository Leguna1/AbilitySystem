#include "ArrowFieldHazard.h"

#include "CombatantComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "FeedbackComponent.h"
#include "FeedbackSubsystem.h"
#include "FeedbackTags.h"
#include "GenericTeamAgentInterface.h"
#include "ImpactGroupSubsystem.h"
#include "ProjectileBase.h"
#include "TimerManager.h"

AArrowFieldHazard::AArrowFieldHazard()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));

	FeedbackComponent = CreateDefaultSubobject<UFeedbackComponent>(TEXT("Feedback"));
}

void AArrowFieldHazard::InitializeField(const FArrowFieldParams& InParams)
{
	Params = InParams;
	Params.Radius = FMath::Max(Params.Radius, 1.0f);

	// Held open until the field ends, so its Thunderclap still belongs to the volley that made it.
	UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this);
	bInImpactGroup = IsValid(ImpactGroups) && ImpactGroups->AddMember(Params.ImpactGroup);

	GetWorldTimerManager().SetTimer(
		CollectionTimerHandle,
		this,
		&AArrowFieldHazard::HandleCollectionTimeout,
		FMath::Max(Params.CollectionTime, 0.1f),
		false
	);
}

void AArrowFieldHazard::RegisterProjectile(AProjectileBase* Arrow)
{
	if (!IsValid(Arrow) || bEnded || bRainFinished)
	{
		return;
	}

	TrackedArrows.Add(Arrow, Arrow->GetFlightSerial());
	Arrow->OnProjectileHit.AddUniqueDynamic(this, &AArrowFieldHazard::HandleArrowHit);
}

void AArrowFieldHazard::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCollecting();
	LeaveImpactGroup();
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void AArrowFieldHazard::HandleArrowHit(AProjectileBase* Arrow, AActor* HitActor, float Damage, const FHitResult& Hit)
{
	const uint32* RegisteredSerial = TrackedArrows.Find(Arrow);
	const bool bSameFlight = RegisteredSerial && IsValid(Arrow) && *RegisteredSerial == Arrow->GetFlightSerial();

	// First hit only: a piercing arrow reports every target it passes through.
	UnbindArrow(Arrow);

	// A pooled arrow may have been fired again by something else since it was registered.
	if (bSameFlight && !bEnded && !bFieldActive)
	{
		const FVector Landing = Hit.ImpactPoint.IsNearlyZero()
			? Arrow->GetActorLocation()
			: FVector(Hit.ImpactPoint);

		StartField(Landing);
	}

	if (TrackedArrows.IsEmpty())
	{
		HandleRainFinished();
	}
}

void AArrowFieldHazard::StartField(const FVector& FirstLanding)
{
	bFieldActive = true;
	FieldCenter = Params.bCenterOnFirstLanding ? FirstLanding : Params.Center;
	SetActorLocation(FieldCenter);

	FeedbackComponent->StartLoop(FeedbackTags::Cue_Hazard_Field, GetRootComponent());
	FeedbackComponent->SetLoopFloat(FeedbackTags::Cue_Hazard_Field, RadiusParameter, Params.Radius);

	OnFieldStarted(FieldCenter, Params.Radius);

	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.SetTimer(LifetimeTimerHandle, this, &AArrowFieldHazard::EndField, FMath::Max(Params.Duration, 0.1f), false);
	TimerManager.SetTimer(PulseTimerHandle, this, &AArrowFieldHazard::Pulse, PulseInterval, true);

	// Statuses land with the rain, not one pulse later.
	Pulse();
}

bool AArrowFieldHazard::CanAffect(const AActor* Actor) const
{
	const AActor* InstigatorActor = GetInstigator();

	if (!IsValid(Actor) || Actor == this)
	{
		return false;
	}

	if (Actor == InstigatorActor)
	{
		return bAffectsInstigator;
	}

	if (!bAffectsFriendlies && IsValid(InstigatorActor) &&
		FGenericTeamId::GetAttitude(InstigatorActor, Actor) == ETeamAttitude::Friendly)
	{
		return false;
	}

	const UCombatantComponent* Combatant = Actor->FindComponentByClass<UCombatantComponent>();
	return IsValid(Combatant) && !Combatant->IsDead();
}

void AArrowFieldHazard::GatherAffectedActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	// A cylinder: a sphere wide enough to cover it, then a flat-distance and height check.
	const float SearchRadius = FMath::Sqrt(FMath::Square(Params.Radius) + FMath::Square(HalfHeight));
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArrowFieldPulse), false, this);
	TArray<FOverlapResult> Overlaps;

	World->OverlapMultiByObjectType(
		Overlaps,
		FieldCenter,
		FQuat::Identity,
		FCollisionObjectQueryParams(AffectedObjectChannel),
		FCollisionShape::MakeSphere(SearchRadius),
		QueryParams
	);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();

		if (!IsValid(Actor) || OutActors.Contains(Actor))
		{
			continue;
		}

		const FVector Offset = Actor->GetActorLocation() - FieldCenter;

		if (FVector2D(Offset).SizeSquared() > FMath::Square(Params.Radius) || FMath::Abs(Offset.Z) > HalfHeight)
		{
			continue;
		}

		if (CanAffect(Actor))
		{
			OutActors.Add(Actor);
		}
	}

	if (bDrawDebugField)
	{
		DrawDebugCylinder(World, FieldCenter - FVector(0.0f, 0.0f, HalfHeight), FieldCenter + FVector(0.0f, 0.0f, HalfHeight),
			Params.Radius, 32, FColor::Cyan, false, PulseInterval);
	}
}

void AArrowFieldHazard::Pulse()
{
	if (bEnded || !bFieldActive)
	{
		return;
	}

	TArray<AActor*> Affected;
	GatherAffectedActors(Affected);

	AActor* InstigatorActor = GetInstigator();

	for (AActor* Actor : Affected)
	{
		UCombatantComponent* Combatant = Actor->FindComponentByClass<UCombatantComponent>();

		// Same source each pulse, so standing in the field refreshes the status instead of stacking it.
		for (const FStatusApplication& Status : Params.Statuses)
		{
			Combatant->ApplyStatus(Status, InstigatorActor);
		}

		OnActorAffected(Actor);
	}
}

void AArrowFieldHazard::HandleCollectionTimeout()
{
	StopCollecting();

	HandleRainFinished();
}

void AArrowFieldHazard::HandleRainFinished()
{
	if (bRainFinished || bEnded)
	{
		return;
	}

	bRainFinished = true;
	GetWorldTimerManager().ClearTimer(CollectionTimerHandle);

	// Every arrow is accounted for and none counted as a landing: no field.
	if (!bFieldActive)
	{
		EndField();
		return;
	}

	if (Params.bThunderclap)
	{
		Thunderclap();
	}
}

void AArrowFieldHazard::Thunderclap()
{
	TArray<AActor*> Affected;
	GatherAffectedActors(Affected);

	AActor* InstigatorActor = GetInstigator();
	UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this);

	for (AActor* Actor : Affected)
	{
		const FVector StrikeLocation = UFeedbackSubsystem::ResolveStrikeLocation(Actor, FHitResult(), FieldCenter);

		// Poise only: the arrows already did the damage and applied their statuses.
		FAbilityPayload Payload;
		Params.ThunderclapHit.ApplyTo(Payload);
		Payload.Damage = 0.0f;
		Payload.Statuses.Reset();
		Payload.Instigator = InstigatorActor;
		Payload.Causer = this;
		Payload.Hit.ImpactPoint = StrikeLocation;
		Payload.Hit.Location = StrikeLocation;
		Payload.Hit.ImpactNormal = (Actor->GetActorLocation() - FieldCenter).GetSafeNormal2D();

		const bool bAccepted = UCombatantComponent::DeliverPayload(Actor, Payload);

		if (IsValid(ImpactGroups))
		{
			FImpactReport Report;
			Report.Group = Params.ImpactGroup;
			Report.Result = bAccepted ? EImpactResult::Hit : EImpactResult::Miss;
			Report.Location = StrikeLocation;
			Report.Rotation = FVector(Payload.Hit.ImpactNormal).Rotation();
			Report.FeedbackSet = FeedbackComponent->GetActiveSet();
			Report.FallbackSet = FeedbackComponent->GetFallbackSet();
			Report.HitCue = FeedbackTags::Cue_Hazard_Hit;
			Report.MissCue = FeedbackTags::Cue_Hazard_Hit;
			Report.Instigator = InstigatorActor;
			Report.Target = Actor;
			Report.Hit = Payload.Hit;

			ImpactGroups->PlayImpact(Report);
		}
	}

	FFeedbackCueContext Context;
	Context.Location = FieldCenter;
	Context.Instigator = InstigatorActor;
	Context.NiagaraFloats.Add(RadiusParameter, Params.Radius);
	FeedbackComponent->PlayCueWithContext(FeedbackTags::Cue_Hazard_Burst, Context);

	OnThunderclap(FieldCenter, Params.Radius);
}

void AArrowFieldHazard::StopCollecting()
{
	TArray<TWeakObjectPtr<AProjectileBase>> Arrows;
	TrackedArrows.GetKeys(Arrows);

	for (const TWeakObjectPtr<AProjectileBase>& Arrow : Arrows)
	{
		UnbindArrow(Arrow.Get());
	}

	TrackedArrows.Reset();
}

void AArrowFieldHazard::UnbindArrow(AProjectileBase* Arrow)
{
	if (IsValid(Arrow))
	{
		Arrow->OnProjectileHit.RemoveDynamic(this, &AArrowFieldHazard::HandleArrowHit);
	}

	TrackedArrows.Remove(Arrow);
}

void AArrowFieldHazard::EndField()
{
	if (bEnded)
	{
		return;
	}

	bEnded = true;
	bFieldActive = false;

	StopCollecting();
	LeaveImpactGroup();
	GetWorldTimerManager().ClearAllTimersForObject(this);

	OnFieldEnded();

	// Let the ground effect fade instead of popping out (its Stop Effect plays now).
	const bool bHadField = FeedbackComponent->IsLoopActive(FeedbackTags::Cue_Hazard_Field);
	FeedbackComponent->StopLoop(FeedbackTags::Cue_Hazard_Field);

	if (bHadField && FadeOutTime > 0.0f)
	{
		SetLifeSpan(FadeOutTime);
		return;
	}

	Destroy();
}

void AArrowFieldHazard::LeaveImpactGroup()
{
	if (!bInImpactGroup)
	{
		return;
	}

	bInImpactGroup = false;

	if (UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this))
	{
		ImpactGroups->ResolveMember(Params.ImpactGroup);
	}
}

#if WITH_EDITOR
#include "AbilityValidation.h"

EDataValidationResult AArrowFieldHazard::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Statuses.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("ArrowFieldHazard", "NoStatuses", "No Statuses: the field only matters for a Thunderclap."));
	}

	return CombineDataValidationResults(Result,
		AbilityValidation::ValidateStatusSpecs(Statuses, TEXT("Statuses"), Context));
}
#endif
