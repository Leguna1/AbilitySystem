#include "EnemyAIController.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"
#include "BaseEnemy.h"
#include "CombatantComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"

AEnemyAIController::AEnemyAIController()
{
	Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));

	// Radii are replaced by the enemy's aggro radius on possess; these are fallbacks.
	SightConfig->SightRadius = 1200.0f;
	SightConfig->LoseSightRadius = 1500.0f;
	SightConfig->PeripheralVisionAngleDegrees = 70.0f;
	SightConfig->SetMaxAge(ForgetTargetAfter);

	// Only hostiles (a different team) are noticed.
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
		// Perception filters by this controller's team, so it mirrors the enemy's.
		SetGenericTeamId(Enemy->GetGenericTeamId());

		SightConfig->SightRadius = Enemy->GetAggroRadius();
		SightConfig->LoseSightRadius = Enemy->GetAggroRadius() * 1.25f;
		SightConfig->PointOfViewBackwardOffset = Enemy->GetSightBackwardOffset();
		SightConfig->NearClippingRadius = Enemy->GetSightBackwardOffset();
		Perception->ConfigureSense(*SightConfig);
		Perception->RequestStimuliListenerUpdate();
		StartIdleWait();
	}

	Perception->OnTargetPerceptionUpdated.AddUniqueDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);

	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.SetTimer(EvaluateTimerHandle, this, &AEnemyAIController::EvaluateTarget, EvaluateInterval, true);

	// Random first delay so a pack doesn't think on the same frame.
	TimerManager.SetTimer(ThinkTimerHandle, this, &AEnemyAIController::Think, ThinkInterval, true,
		FMath::FRandRange(0.0f, ThinkInterval));
}

void AEnemyAIController::OnUnPossess()
{
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(EvaluateTimerHandle);
	TimerManager.ClearTimer(ThinkTimerHandle);

	Perception->OnTargetPerceptionUpdated.RemoveDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
	
	SetReturningHome(false);
	ClearTarget();

	Super::OnUnPossess();
}

/* ============================== Target ============================== */

void AEnemyAIController::SetCurrentTarget(AActor* NewTarget)
{
	if (!IsValidTarget(NewTarget) || NewTarget == CurrentTarget.Get())
	{
		return;
	}

	CurrentTarget = NewTarget;
	LastSeenTime = GetWorldTime();

	// Focus turns the enemy to face its target while it acts.
	SetFocus(NewTarget);
	OnTargetChanged.Broadcast(NewTarget);
}

void AEnemyAIController::ClearTarget()
{
	if (!CurrentTarget.IsValid())
	{
		return;
	}

	CurrentTarget.Reset();
	ClearFocus(EAIFocusPriority::Gameplay);
	OnTargetChanged.Broadcast(nullptr);
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

	if (Actor == CurrentTarget.Get())
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

	if (AActor* Target = CurrentTarget.Get())
	{
		if (!IsValidTarget(Target))
		{
			// Killed (or otherwise no longer a valid target).
			ClearTarget();
			bLostTarget = true;
		}
		else if (DistanceFromHome > Enemy->GetLeashRadius())
		{
			// Leash: dragged too far from home, give up and go back. Think() walks it home.
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

bool AEnemyAIController::IsValidTarget(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor == GetPawn() || GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return false;
	}

	const UCombatantComponent* Combatant = Actor->FindComponentByClass<UCombatantComponent>();
	return IsValid(Combatant) && !Combatant->IsDead();
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

double AEnemyAIController::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

/* ============================== Behavior ============================== */

void AEnemyAIController::Think()
{
	ABaseEnemy* EnemyPawn = Enemy.Get();

	if (!IsValid(EnemyPawn))
	{
		return;
	}

	if (EnemyPawn->IsDead())
	{
		GetWorldTimerManager().ClearTimer(ThinkTimerHandle);
		SetBehaviorState(EEnemyBehaviorState::Dead);
		return;
	}

	UAbilityComponent* Abilities = EnemyPawn->GetAbilityComponent();

	// An attack or hit reaction is playing: let it finish.
	if (!IsValid(Abilities) || Abilities->HasActiveAbility())
	{
		return;
	}

	// An attack just finished: start recovery before the next one.
	if (BehaviorState == EEnemyBehaviorState::Attacking)
	{
		NextAttackTime = GetWorldTime() + FMath::FRandRange(EnemyPawn->AttackRecoveryMin, EnemyPawn->AttackRecoveryMax);
		SetBehaviorState(EEnemyBehaviorState::Engaging);
	}

	if (IsReturningHome())
	{
		ThinkReturnHome();
		UpdateCombatState();
	}
	else if (AActor* Target = GetCurrentTarget())
	{
		ThinkCombat(Abilities, Target);
		UpdateCombatState();
	}
	else
	{
		ThinkIdle();
		UpdateCombatState();
	}

	if (bDrawBehaviorDebug)
	{
		DrawDebugString(GetWorld(), FVector(0.0f, 0.0f, 120.0f), UEnum::GetDisplayValueAsText(BehaviorState).ToString(),
			EnemyPawn, FColor::Yellow, ThinkInterval, true);

		DrawDebugCircle(GetWorld(), EnemyPawn->GetHomeLocation(), EnemyPawn->RoamRadius, 32, FColor::Green,
			false, ThinkInterval, 0, 2.0f, FVector::ForwardVector, FVector::RightVector, false);
	}
}



void AEnemyAIController::ThinkReturnHome()
{
	PlannedAttackIndex = INDEX_NONE;

	if (BehaviorState == EEnemyBehaviorState::ReturningHome)
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
		SetBehaviorState(EEnemyBehaviorState::ReturningHome);
	}
	else
	{
		// Already there, or no path home: stop trying.
		SetReturningHome(false);
	}
}
void AEnemyAIController::ThinkCombat(UAbilityComponent* Abilities, AActor* Target)
{
	const float Gap = Enemy->GetEdgeDistanceTo(Target);

	// Re-plan when the plan can't be used from here anymore, or something better became usable
	// (a big spell came off cooldown). Otherwise keep it, so the chase doesn't flip between attacks.
	int32 HighestPriority = 0;

	if (!FindHighestUsablePriority(Abilities, Gap, HighestPriority))
	{
		PlannedAttackIndex = INDEX_NONE;
	}
	else if (!IsAttackUsable(Abilities, PlannedAttackIndex, Gap) ||
		Enemy->Attacks[PlannedAttackIndex].Priority < HighestPriority)
	{
		PlannedAttackIndex = PickAttack(Abilities, Gap);
	}

	// Nothing usable from here: hold position and keep facing the target.
	if (PlannedAttackIndex == INDEX_NONE)
	{
		if (BehaviorState != EEnemyBehaviorState::Engaging)
		{
			StopMovement();
			SetBehaviorState(EEnemyBehaviorState::Engaging);
		}
		return;
	}

	const FEnemyAttack& PlannedAttack = Enemy->Attacks[PlannedAttackIndex];
	const bool bInRange = Gap <= PlannedAttack.MaxRange;
	const bool bHasLineOfSight = !PlannedAttack.bRequiresLineOfSight || LineOfSightTo(Target);

	// Out of range: close in. In range but blocked: walk in until the view is clear.
	if (!bInRange || !bHasLineOfSight)
	{
		const float ChaseAcceptance = bInRange
			? LineOfSightChaseRadius
			: PlannedAttack.MaxRange * ChaseRangeFraction;

		const bool bNeedsNewMove =
			BehaviorState != EEnemyBehaviorState::Chasing ||
			GetMoveStatus() != EPathFollowingStatus::Moving ||
			!FMath::IsNearlyEqual(ChaseAcceptance, LastChaseAcceptance);

		if (bNeedsNewMove)
		{
			// With bStopOnOverlap (the default) the acceptance radius is measured edge to edge, like GetEdgeDistanceTo.
			MoveToActor(Target, ChaseAcceptance);
			LastChaseAcceptance = ChaseAcceptance;
			SetBehaviorState(EEnemyBehaviorState::Chasing);
		}
		return;
	}

	// In range with a clear view.
	if (GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		StopMovement();
	}
	SetBehaviorState(EEnemyBehaviorState::Engaging);

	if (GetWorldTime() < NextAttackTime || !IsFacing(Target))
	{
		return;
	}

	if (Abilities->TryActivateAbility(PlannedAttack.AbilityTag))
	{
		SetBehaviorState(EEnemyBehaviorState::Attacking);
	}

	// Pick fresh next time, whether it activated or not.
	PlannedAttackIndex = INDEX_NONE;
}

bool AEnemyAIController::IsAttackUsable(const UAbilityComponent* Abilities, const int32 AttackIndex, const float Gap) const
{
	if (!Enemy->Attacks.IsValidIndex(AttackIndex))
	{
		return false;
	}

	const FEnemyAttack& Candidate = Enemy->Attacks[AttackIndex];

	return !Abilities->IsAbilityOnCooldown(Candidate.AbilityTag) &&
		Gap >= Candidate.MinRange &&
		(Candidate.bChaseIntoRange || Gap <= Candidate.MaxRange);
}

bool AEnemyAIController::FindHighestUsablePriority(const UAbilityComponent* Abilities, const float Gap, int32& OutPriority) const
{
	bool bFound = false;

	for (int32 Index = 0; Index < Enemy->Attacks.Num(); ++Index)
	{
		if (!IsAttackUsable(Abilities, Index, Gap))
		{
			continue;
		}

		const int32 CandidatePriority = Enemy->Attacks[Index].Priority;
		OutPriority = bFound ? FMath::Max(OutPriority, CandidatePriority) : CandidatePriority;
		bFound = true;
	}

	return bFound;
}

int32 AEnemyAIController::PickAttack(const UAbilityComponent* Abilities, const float Gap) const
{
	int32 HighestPriority = 0;

	if (!FindHighestUsablePriority(Abilities, Gap, HighestPriority))
	{
		return INDEX_NONE;
	}

	const TArray<FEnemyAttack>& EnemyAttacks = Enemy->Attacks;

	// Only the best priority competes; Weight decides among those.
	const auto IsCandidate = [this, Abilities, Gap, HighestPriority, &EnemyAttacks](const int32 Index)
	{
		return EnemyAttacks[Index].Priority == HighestPriority && IsAttackUsable(Abilities, Index, Gap);
	};

	float TotalWeight = 0.0f;

	for (int32 Index = 0; Index < EnemyAttacks.Num(); ++Index)
	{
		if (IsCandidate(Index))
		{
			TotalWeight += EnemyAttacks[Index].Weight;
		}
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);

	for (int32 Index = 0; Index < EnemyAttacks.Num(); ++Index)
	{
		if (IsCandidate(Index))
		{
			Roll -= EnemyAttacks[Index].Weight;

			if (Roll <= 0.0f)
			{
				return Index;
			}
		}
	}

	return INDEX_NONE;
}

bool AEnemyAIController::IsFacing(const AActor* Target) const
{
	const FVector Forward = Enemy->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToTarget = (Target->GetActorLocation() - Enemy->GetActorLocation()).GetSafeNormal2D();

	return FVector::DotProduct(Forward, ToTarget) >= FMath::Cos(FMath::DegreesToRadians(Enemy->MaxAttackAngle));
}

void AEnemyAIController::SetBehaviorState(EEnemyBehaviorState NewState)
{
	BehaviorState = NewState;
}
void AEnemyAIController::ThinkIdle()
{
	PlannedAttackIndex = INDEX_NONE;

	switch (BehaviorState)
	{
	case EEnemyBehaviorState::Roaming:
		// Still walking to the roam point.
		if (GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			return;
		}

		// Arrived (or the path failed): pause before the next walk.
		StartIdleWait();
		SetBehaviorState(EEnemyBehaviorState::Idle);
		return;

	case EEnemyBehaviorState::Idle:
		break;

	default:
		// Just out of combat or back home: stop and pause before wandering.
		StopMovement();
		StartIdleWait();
		SetBehaviorState(EEnemyBehaviorState::Idle);
		return;
	}

	if (Enemy->RoamRadius <= 0.0f || GetWorldTime() < NextRoamTime)
	{
		return;
	}

	FVector RoamPoint;

	if (FindRoamPoint(RoamPoint) && MoveToLocation(RoamPoint) == EPathFollowingRequestResult::RequestSuccessful)
	{
		SetBehaviorState(EEnemyBehaviorState::Roaming);
	}
	else
	{
		// No usable point this time; try again after another pause.
		StartIdleWait();
	}
}

void AEnemyAIController::StartIdleWait()
{
	NextRoamTime = GetWorldTime() + FMath::FRandRange(Enemy->RoamWaitMin, Enemy->RoamWaitMax);
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
void AEnemyAIController::SetReturningHome(bool bNewReturningHome)
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
void AEnemyAIController::UpdateCombatState()
{
	if (ABaseEnemy* EnemyPawn = Enemy.Get())
	{
		EnemyPawn->SetInCombat(IsValid(GetCurrentTarget()) && !bReturningHome);
	}
}