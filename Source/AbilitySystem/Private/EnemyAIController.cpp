#include "EnemyAIController.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"
#include "BaseEnemy.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"

AEnemyAIController::AEnemyAIController()
{
	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// Radii are replaced by the enemy's aggro radius on possess; these are fallbacks.
	SightConfig->SightRadius = 1200.0f;
	SightConfig->LoseSightRadius = 1500.0f;
	SightConfig->PeripheralVisionAngleDegrees = 70.0f;
	SightConfig->SetMaxAge(ForgetTargetAfter);

	// Only hostiles (a different team) are noticed: the player and the player's companion.
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;

	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*Perception);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	Enemy = Cast<ABaseEnemy>(InPawn);

	if (Enemy.IsValid())
	{
		SightConfig->SightRadius = Enemy->GetAggroRadius();
		SightConfig->LoseSightRadius = Enemy->GetAggroRadius() * 1.25f;
		SightConfig->PointOfViewBackwardOffset = Enemy->GetSightBackwardOffset();
		SightConfig->NearClippingRadius = Enemy->GetSightBackwardOffset();
		Perception->ConfigureSense(*SightConfig);
		Perception->RequestStimuliListenerUpdate();
		StartIdleWait();
	}

	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
}

void AEnemyAIController::OnUnPossess()
{
	Perception->OnTargetPerceptionUpdated.RemoveDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
	SetReturningHome(false);

	Super::OnUnPossess();
}

/* ============================== Target ============================== */

void AEnemyAIController::OnTargetAcquired(AActor* NewTarget)
{
	LastSeenTime = GetWorldTime();
}

void AEnemyAIController::NotifyAttackedBy(AActor* Attacker)
{
	if (!HasTarget() && !bReturningHome)
	{
		SetCurrentTarget(Attacker);
	}
}

void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, const FAIStimulus Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed() || !IsValidTarget(Actor))
	{
		return;
	}

	if (Actor == GetCurrentTarget())
	{
		LastSeenTime = GetWorldTime();
		return;
	}

	// Losing sight is handled by EvaluateTarget's forget timer, so ducking behind a pillar doesn't drop the target instantly.
	if (!HasTarget() && !bReturningHome)
	{
		SetCurrentTarget(Actor);
	}
}

void AEnemyAIController::EvaluateTarget()
{
	if (!Enemy.IsValid() || Enemy->IsDead())
	{
		ClearTarget();
		return;
	}

	const float DistanceFromHome = FVector::Dist(Enemy->GetActorLocation(), Enemy->GetHomeLocation());

	if (bReturningHome)
	{
		SetReturningHome(DistanceFromHome > HomeReachedDistance);
		return;
	}

	bool bLostTarget = false;

	if (AActor* Target = GetCurrentTarget())
	{
		if (!IsValidTarget(Target))
		{
			// Killed (or otherwise no longer a valid target).
			ClearTarget();
			bLostTarget = true;
		}
		else if (DistanceFromHome > Enemy->GetLeashRadius())
		{
			// Leash: dragged too far from home, give up and go back.
			ClearTarget();
			SetReturningHome(true);
			return;
		}
		else
		{
			static const FAISenseID SightId = UAISense::GetSenseID<UAISense_Sight>();

			if (Perception->HasActiveStimulus(*Target, SightId))
			{
				LastSeenTime = GetWorldTime();
				return;
			}

			if (GetWorldTime() - LastSeenTime <= ForgetTargetAfter)
			{
				return;
			}

			// Out of sight too long: forgotten.
			ClearTarget();
			bLostTarget = true;
		}
	}

	// No target: the nearest hostile in sight, else one close enough to be felt.
	AActor* NewTarget = FindNearestSeenTarget();

	if (!NewTarget)
	{
		NewTarget = FindNearestTargetInProximity();
	}

	if (NewTarget)
	{
		SetCurrentTarget(NewTarget);
		return;
	}

	// The fight is over and nobody else is around: walk home instead of idling wherever it ended.
	if (bLostTarget && DistanceFromHome > GetHomeAreaRadius())
	{
		SetReturningHome(true);
	}
}

float AEnemyAIController::GetHomeAreaRadius() const
{
	return Enemy.IsValid() ? FMath::Max(HomeReachedDistance, Enemy->RoamRadius) : HomeReachedDistance;
}

AActor* AEnemyAIController::FindNearestSeenTarget() const
{
	const APawn* ControlledPawn = GetPawn();

	if (!IsValid(ControlledPawn))
	{
		return nullptr;
	}

	TArray<AActor*> Seen;
	Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Seen);

	AActor* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();

	for (AActor* Actor : Seen)
	{
		if (!IsValidTarget(Actor))
		{
			continue;
		}

		const float Distance = FVector::DistSquared(Actor->GetActorLocation(), ControlledPawn->GetActorLocation());

		if (Distance < NearestDistance)
		{
			Nearest = Actor;
			NearestDistance = Distance;
		}
	}

	return Nearest;
}

AActor* AEnemyAIController::FindNearestTargetInProximity() const
{
	const APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	const float Radius = Enemy.IsValid() ? Enemy->GetProximityAwarenessRadius() : 0.0f;

	if (!IsValid(ControlledPawn) || !IsValid(World) || Radius <= 0.0f)
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyProximity), false, ControlledPawn);
	TArray<FOverlapResult> Overlaps;

	World->OverlapMultiByObjectType(
		Overlaps,
		ControlledPawn->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		QueryParams
	);

	AActor* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();

		if (!IsValidTarget(Actor))
		{
			continue;
		}

		const float Distance = FVector::DistSquared(Actor->GetActorLocation(), ControlledPawn->GetActorLocation());

		if (Distance < NearestDistance)
		{
			Nearest = Actor;
			NearestDistance = Distance;
		}
	}

	return Nearest;
}

/* ============================== Behavior ============================== */

bool AEnemyAIController::ThinkOverride()
{
	if (!bReturningHome)
	{
		return false;
	}

	ThinkReturnHome();
	return true;
}

void AEnemyAIController::ThinkWithoutTarget()
{
	ThinkIdle();
}

void AEnemyAIController::ThinkReturnHome()
{
	ResetAttackPlan();

	if (BehaviorState == EAIBehaviorState::ReturningHome)
	{
		// The walk home has finished (arrived, or got as close as it can): it's home.
		if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			SetReturningHome(false);
		}
		return;
	}

	// bStopOnOverlap = false: a plain acceptance radius. With it on, the capsule radius is added
	// and big enemies stop short of home.
	const EPathFollowingRequestResult::Type MoveResult =
		MoveToLocation(Enemy->GetHomeLocation(), HomeReachedDistance * 0.5f, /*bStopOnOverlap*/ false);

	if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		SetBehaviorState(EAIBehaviorState::ReturningHome);
	}
	else
	{
		// Already there, or no path home: stop trying.
		SetReturningHome(false);
	}
}

void AEnemyAIController::ThinkIdle()
{
	ResetAttackPlan();

	switch (BehaviorState)
	{
	case EAIBehaviorState::Roaming:
		// Still walking to the roam point.
		if (GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			return;
		}

		// Arrived (or the path failed): pause before the next walk.
		StartIdleWait();
		SetBehaviorState(EAIBehaviorState::Idle);
		return;

	case EAIBehaviorState::Idle:
		break;

	default:
		// Just out of combat or back home: stop and pause before wandering.
		StopMovement();
		StartIdleWait();
		SetBehaviorState(EAIBehaviorState::Idle);
		return;
	}

	if (Enemy->RoamRadius <= 0.0f || GetWorldTime() < NextRoamTime)
	{
		return;
	}

	FVector RoamPoint;

	if (FindRoamPoint(RoamPoint) && MoveToLocation(RoamPoint) == EPathFollowingRequestResult::RequestSuccessful)
	{
		SetBehaviorState(EAIBehaviorState::Roaming);
	}
	else
	{
		// No usable point this time; try again after another pause.
		StartIdleWait();
	}
}

void AEnemyAIController::StartIdleWait()
{
	if (Enemy.IsValid())
	{
		NextRoamTime = GetWorldTime() + FMath::FRandRange(Enemy->RoamWaitMin, Enemy->RoamWaitMax);
	}
}

bool AEnemyAIController::FindRoamPoint(FVector& OutPoint) const
{
	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (!IsValid(NavSystem))
	{
		return false;
	}

	// Home is the capsule center; the nav mesh is at the feet. Big enemies are too tall for the default query to bridge.
	const FVector HomeFeet = Enemy->GetHomeLocation() - FVector(0.0f, 0.0f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	FNavLocation HomeOnNavMesh;

	if (!NavSystem->ProjectPointToNavigation(HomeFeet, HomeOnNavMesh))
	{
		return false;
	}

	// A few tries for a point worth walking to, not one right where we stand.
	const float MinTravelDistance = Enemy->RoamRadius * 0.3f;

	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		FNavLocation Candidate;

		if (NavSystem->GetRandomReachablePointInRadius(HomeOnNavMesh.Location, Enemy->RoamRadius, Candidate)
			&& FVector::Dist2D(Candidate.Location, Enemy->GetActorLocation()) >= MinTravelDistance)
		{
			OutPoint = Candidate.Location;
			return true;
		}
	}

	return false;
}

void AEnemyAIController::SetReturningHome(const bool bNewReturningHome)
{
	if (bReturningHome == bNewReturningHome)
	{
		return;
	}

	bReturningHome = bNewReturningHome;

	UAbilityComponent* Abilities = Enemy.IsValid() ? Enemy->GetAbilityComponent() : nullptr;

	if (!IsValid(Abilities))
	{
		return;
	}

	if (bReturningHome)
	{
		Abilities->AddLooseOwnerTag(AbilitySystemTags::State_Invulnerable);
	}
	else
	{
		Abilities->RemoveLooseOwnerTag(AbilitySystemTags::State_Invulnerable);
	}
}

void AEnemyAIController::DrawBehaviorDebug() const
{
	if (Enemy.IsValid())
	{
		DrawDebugCircle(GetWorld(), Enemy->GetHomeLocation(), Enemy->RoamRadius, 32, FColor::Green,
			false, ThinkInterval, 0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
	}
}
