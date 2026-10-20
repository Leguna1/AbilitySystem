#include "CombatAIController.h"

#include "AbilityComponent.h"
#include "BaseAICharacter.h"
#include "CombatantComponent.h"
#include "DrawDebugHelpers.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

void ACombatAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	AICharacter = Cast<ABaseAICharacter>(InPawn);

	if (AICharacter.IsValid())
	{
		// Perception and attitude checks use this controller's team, so it mirrors the character's.
		SetGenericTeamId(AICharacter->GetGenericTeamId());
	}

	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.SetTimer(EvaluateTimerHandle, this, &ACombatAIController::EvaluateTarget, EvaluateInterval, true);

	// Random first delay so a pack doesn't think on the same frame.
	TimerManager.SetTimer(ThinkTimerHandle, this, &ACombatAIController::Think, ThinkInterval, true,
		FMath::FRandRange(0.0f, ThinkInterval));
}

void ACombatAIController::OnUnPossess()
{
	FTimerManager& TimerManager = GetWorldTimerManager();
	TimerManager.ClearTimer(EvaluateTimerHandle);
	TimerManager.ClearTimer(ThinkTimerHandle);

	ClearTarget();

	Super::OnUnPossess();
}

/* ============================== Target ============================== */

void ACombatAIController::SetCurrentTarget(AActor* NewTarget)
{
	if (!IsValidTarget(NewTarget) || NewTarget == CurrentTarget.Get())
	{
		return;
	}

	CurrentTarget = NewTarget;
	PlannedAttackIndex = INDEX_NONE;

	// Focus turns the character to face its target while it acts.
	SetFocus(NewTarget);
	OnTargetAcquired(NewTarget);
	OnTargetChanged.Broadcast(NewTarget);
}

void ACombatAIController::ClearTarget()
{
	ResetAttackPlan();

	if (!CurrentTarget.IsValid())
	{
		return;
	}

	CurrentTarget.Reset();
	ClearFocus(EAIFocusPriority::Gameplay);
	OnTargetChanged.Broadcast(nullptr);
}

void ACombatAIController::NotifyAttackedBy(AActor* Attacker)
{
}

bool ACombatAIController::IsAbilityReady(const UAbilityComponent* Abilities, const FGameplayTag AbilityId)
{
	return IsValid(Abilities) &&
		Abilities->GetAbilityRank(Abilities->FindAbilityClassById(AbilityId)) > 0 &&
		!Abilities->IsAbilityOnCooldown(AbilityId);
}

void ACombatAIController::ResetAttackPlan()
{
	PlannedAttackIndex = INDEX_NONE;
}

bool ACombatAIController::IsValidTarget(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor == GetPawn() || GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return false;
	}

	const UCombatantComponent* Combatant = Actor->FindComponentByClass<UCombatantComponent>();
	return IsValid(Combatant) && !Combatant->IsDead();
}

ABaseAICharacter* ACombatAIController::GetAICharacter() const
{
	return AICharacter.Get();
}

double ACombatAIController::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

/* ============================== Behavior ============================== */

void ACombatAIController::Think()
{
	ABaseAICharacter* Fighter = AICharacter.Get();

	if (!IsValid(Fighter))
	{
		return;
	}

	if (Fighter->IsDead())
	{
		GetWorldTimerManager().ClearTimer(ThinkTimerHandle);
		SetBehaviorState(EAIBehaviorState::Dead);
		return;
	}

	UAbilityComponent* Abilities = Fighter->GetAbilityComponent();

	// An attack or hit reaction is playing: let it finish.
	if (!IsValid(Abilities) || Abilities->HasActiveAbility())
	{
		return;
	}

	// An attack just finished: start recovery before the next one.
	if (BehaviorState == EAIBehaviorState::Attacking)
	{
		NextAttackTime = GetWorldTime() + FMath::FRandRange(Fighter->AttackRecoveryMin, Fighter->AttackRecoveryMax);
		SetBehaviorState(EAIBehaviorState::Engaging);
	}

	if (!ThinkOverride())
	{
		if (AActor* Target = GetCurrentTarget())
		{
			ThinkCombat(Abilities, Target);
		}
		else
		{
			ThinkWithoutTarget();
		}
	}

	UpdateCombatState();

	if (bDrawBehaviorDebug)
	{
		DrawDebugString(GetWorld(), FVector(0.0f, 0.0f, 120.0f), UEnum::GetDisplayValueAsText(BehaviorState).ToString(),
			Fighter, FColor::Yellow, ThinkInterval, true);

		DrawBehaviorDebug();
	}
}

void ACombatAIController::ThinkCombat(UAbilityComponent* Abilities, AActor* Target)
{
	ABaseAICharacter* Fighter = AICharacter.Get();
	const float Gap = Fighter->GetEdgeDistanceTo(Target);

	// Re-plan when the plan can't be used from here anymore, or something better became usable.
	// Otherwise keep it, so the chase doesn't flip between attacks.
	int32 HighestPriority = 0;

	if (!FindHighestUsablePriority(Abilities, Gap, HighestPriority))
	{
		PlannedAttackIndex = INDEX_NONE;
	}
	else if (!IsAttackUsable(Abilities, PlannedAttackIndex, Gap) ||
		Fighter->Attacks[PlannedAttackIndex].Priority < HighestPriority)
	{
		PlannedAttackIndex = PickAttack(Abilities, Gap);
	}

	// Nothing usable from here: hold position and keep facing the target.
	if (PlannedAttackIndex == INDEX_NONE)
	{
		if (BehaviorState != EAIBehaviorState::Engaging)
		{
			StopMovement();
			SetBehaviorState(EAIBehaviorState::Engaging);
		}
		return;
	}

	const FAIAttack& PlannedAttack = Fighter->Attacks[PlannedAttackIndex];
	const bool bInRange = Gap <= PlannedAttack.MaxRange;
	const bool bHasLineOfSight = !PlannedAttack.bRequiresLineOfSight || LineOfSightTo(Target);

	// Out of range: close in. In range but blocked: walk in until the view is clear.
	if (!bInRange || !bHasLineOfSight)
	{
		const float ChaseAcceptance = bInRange
			? LineOfSightChaseRadius
			: PlannedAttack.MaxRange * ChaseRangeFraction;

		const bool bNeedsNewMove =
			BehaviorState != EAIBehaviorState::Chasing ||
			GetMoveStatus() != EPathFollowingStatus::Moving ||
			!FMath::IsNearlyEqual(ChaseAcceptance, LastChaseAcceptance);

		if (bNeedsNewMove)
		{
			// With bStopOnOverlap (the default) the acceptance radius is measured edge to edge, like GetEdgeDistanceTo.
			MoveToActor(Target, ChaseAcceptance);
			LastChaseAcceptance = ChaseAcceptance;
			SetBehaviorState(EAIBehaviorState::Chasing);
		}
		return;
	}

	// In range with a clear view.
	if (GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		StopMovement();
	}
	SetBehaviorState(EAIBehaviorState::Engaging);

	if (GetWorldTime() < NextAttackTime || !IsFacing(Target))
	{
		return;
	}

	if (Abilities->TryActivateAbility(PlannedAttack.AbilityTag))
	{
		SetBehaviorState(EAIBehaviorState::Attacking);
	}

	// Pick fresh next time, whether it activated or not.
	PlannedAttackIndex = INDEX_NONE;
}

bool ACombatAIController::IsAttackUsable(const UAbilityComponent* Abilities, const int32 AttackIndex, const float Gap) const
{
	const ABaseAICharacter* Fighter = AICharacter.Get();

	if (!IsValid(Fighter) || !Fighter->Attacks.IsValidIndex(AttackIndex))
	{
		return false;
	}

	const FAIAttack& Candidate = Fighter->Attacks[AttackIndex];

	// Granted with a rank, and ready.
	return IsAbilityReady(Abilities, Candidate.AbilityTag) &&
		Gap >= Candidate.MinRange &&
		(Candidate.bChaseIntoRange || Gap <= Candidate.MaxRange);
}

bool ACombatAIController::FindHighestUsablePriority(const UAbilityComponent* Abilities, const float Gap, int32& OutPriority) const
{
	const TArray<FAIAttack>& Attacks = AICharacter->Attacks;
	bool bFound = false;

	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		if (!IsAttackUsable(Abilities, Index, Gap))
		{
			continue;
		}

		const int32 CandidatePriority = Attacks[Index].Priority;
		OutPriority = bFound ? FMath::Max(OutPriority, CandidatePriority) : CandidatePriority;
		bFound = true;
	}

	return bFound;
}

int32 ACombatAIController::PickAttack(const UAbilityComponent* Abilities, const float Gap) const
{
	int32 HighestPriority = 0;

	if (!FindHighestUsablePriority(Abilities, Gap, HighestPriority))
	{
		return INDEX_NONE;
	}

	const TArray<FAIAttack>& Attacks = AICharacter->Attacks;

	// Only the best priority competes; Weight decides among those.
	const auto IsCandidate = [this, Abilities, Gap, HighestPriority, &Attacks](const int32 Index)
	{
		return Attacks[Index].Priority == HighestPriority && IsAttackUsable(Abilities, Index, Gap);
	};

	float TotalWeight = 0.0f;

	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		if (IsCandidate(Index))
		{
			TotalWeight += Attacks[Index].Weight;
		}
	}

	float Roll = FMath::FRandRange(0.0f, TotalWeight);

	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		if (IsCandidate(Index))
		{
			Roll -= Attacks[Index].Weight;

			if (Roll <= 0.0f)
			{
				return Index;
			}
		}
	}

	return INDEX_NONE;
}

bool ACombatAIController::IsFacing(const AActor* Target) const
{
	const ABaseAICharacter* Fighter = AICharacter.Get();
	const FVector Forward = Fighter->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToTarget = (Target->GetActorLocation() - Fighter->GetActorLocation()).GetSafeNormal2D();

	return FVector::DotProduct(Forward, ToTarget) >= FMath::Cos(FMath::DegreesToRadians(Fighter->MaxAttackAngle));
}

void ACombatAIController::UpdateCombatState()
{
	if (ABaseAICharacter* Fighter = AICharacter.Get())
	{
		Fighter->SetInCombat(IsEngaged());
	}
}
