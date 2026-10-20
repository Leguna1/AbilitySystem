#include "CompanionAIController.h"

#include "AbilityComponent.h"
#include "AttributeComponent.h"
#include "BaseCharacter.h"
#include "CombatantComponent.h"
#include "CompanionCharacter.h"
#include "CompanionComponent.h"
#include "CompanionNeedsComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Navigation/PathFollowingComponent.h"
#include "TargetingComponent.h"
#include "TimerManager.h"

ACompanionAIController::ACompanionAIController()
{
	PrimaryActorTick.bCanEverTick = false;
}

/* ============================== Possession ============================== */

void ACompanionAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ACompanionCharacter* Companion = Cast<ACompanionCharacter>(InPawn);

	if (!IsValid(Companion))
	{
		return;
	}

	// Attitude checks (who is hostile) use the controller's team, so it mirrors the companion's (the leader's).
	SetGenericTeamId(Companion->GetGenericTeamId());

	BindToSelf(Companion);
	BindToLeader();

	SetState(ECompanionState::Idle);

	// A random first delay, so two companions never think on the same frame.
	GetWorldTimerManager().SetTimer(ThinkTimerHandle, this, &ACompanionAIController::Think, ThinkInterval, true,
		FMath::FRandRange(0.0f, ThinkInterval));
}

void ACompanionAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimerHandle);

	UnbindFromLeader();
	UnbindFromSelf();
	ClearDecision();

	FightMemory.Reset();
	LeaderAttackers.Reset();
	SelfAttackers.Reset();
	Candidates.Reset();

	Super::OnUnPossess();
}

/* ============================== Queries ============================== */

AActor* ACompanionAIController::GetAbilityTarget_Implementation() const
{
	return Decision.Target.Get();
}

ACompanionCharacter* ACompanionAIController::GetCompanion() const
{
	return Cast<ACompanionCharacter>(GetPawn());
}

ABaseCharacter* ACompanionAIController::GetLeader() const
{
	const ACompanionCharacter* Companion = GetCompanion();
	return IsValid(Companion) ? Companion->GetLeader() : nullptr;
}

AActor* ACompanionAIController::GetLeaderTarget() const
{
	const ABaseCharacter* Leader = GetLeader();
	const UTargetingComponent* Targeting = IsValid(Leader) ? Leader->FindComponentByClass<UTargetingComponent>() : nullptr;
	AActor* LeaderTarget = IsValid(Targeting) ? Targeting->GetCurrentTarget() : nullptr;

	return IsValidTarget(LeaderTarget) ? LeaderTarget : nullptr;
}

bool ACompanionAIController::IsValidTarget(const AActor* Actor) const
{
	if (!IsValid(Actor) || Actor == GetPawn() || GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Hostile)
	{
		return false;
	}

	const UCombatantComponent* Combatant = Actor->FindComponentByClass<UCombatantComponent>();
	return IsValid(Combatant) && !Combatant->IsDead();
}

int32 ACompanionAIController::CountHostilesNear(const FVector& Center, const float Radius) const
{
	UWorld* World = GetWorld();

	if (!IsValid(World) || Radius <= 0.0f)
	{
		return 0;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CompanionCount), false, GetPawn());
	TArray<FOverlapResult> Overlaps;

	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius), QueryParams);

	TSet<const AActor*> Counted;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* Hostile = Overlap.GetActor();

		if (IsValidTarget(Hostile))
		{
			Counted.Add(Hostile);
		}
	}

	return Counted.Num();
}

bool ACompanionAIController::IsThreatening(const AActor* Hostile, const AActor* Victim, const float Within) const
{
	if (!IsValid(Hostile) || !IsValid(Victim))
	{
		return false;
	}

	const TMap<TWeakObjectPtr<AActor>, double>& Attackers = Victim == GetPawn() ? SelfAttackers : LeaderAttackers;

	if (const double* LastHit = Attackers.Find(const_cast<AActor*>(Hostile)))
	{
		if (GetWorldTime() - *LastHit <= Within)
		{
			return true;
		}
	}

	// Going for them right now, by its AI's choice of target.
	const APawn* HostilePawn = Cast<APawn>(Hostile);
	AController* HostileController = IsValid(HostilePawn) ? HostilePawn->GetController() : nullptr;

	return IsValid(HostileController) &&
		HostileController->Implements<UAbilityTargetSource>() &&
		IAbilityTargetSource::Execute_GetAbilityTarget(HostileController) == Victim;
}

TArray<AActor*> ACompanionAIController::GetFightEnemies() const
{
	TArray<AActor*> Enemies;

	for (const TWeakObjectPtr<AActor>& Candidate : Candidates)
	{
		if (AActor* Enemy = Candidate.Get())
		{
			Enemies.Add(Enemy);
		}
	}

	return Enemies;
}

float ACompanionAIController::GetPurposeMultiplier(const ECompanionPurpose Purpose) const
{
	const ACompanionCharacter* Companion = GetCompanion();
	const UCompanionNeedsComponent* Needs = IsValid(Companion) ? Companion->GetNeedsComponent() : nullptr;

	return IsValid(Needs) ? Needs->GetNeedMultiplier(Purpose) : 1.0f;
}

double ACompanionAIController::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

/* ============================== Recall ============================== */

void ACompanionAIController::Recall()
{
	ClearDecision();
	FightMemory.Reset();
	Candidates.Reset();
	StopMovement();
	SetState(ECompanionState::Regrouping);
}

void ACompanionAIController::ClearDecision()
{
	Decision.Reset();
	LastChaseAcceptance = -1.0f;
	ClearFocus(EAIFocusPriority::Gameplay);
}

/* ============================== Think ============================== */

void ACompanionAIController::Think()
{
	ACompanionCharacter* Companion = GetCompanion();
	const ABaseCharacter* Leader = GetLeader();

	if (!IsValid(Companion) || Companion->IsDead())
	{
		return;
	}

	BindToLeader();

	const UAbilityComponent* Abilities = Companion->GetAbilityComponent();

	// Busy with an ability or a hit reaction: let it play out.
	if (!IsValid(Abilities) || Abilities->HasActiveAbility())
	{
		return;
	}

	// Just finished an action: a short breath before the next one.
	if (State == ECompanionState::Acting)
	{
		NextActionTime = GetWorldTime() + FMath::FRandRange(Companion->AttackRecoveryMin, Companion->AttackRecoveryMax);
		SetState(ECompanionState::Engaging);
	}

	if (!IsValid(Leader) || Leader->IsDead())
	{
		ClearDecision();
		StopMovement();
		SetState(ECompanionState::Idle);
		return;
	}

	// Dragged too far from the leader: drop everything and come back.
	if (State != ECompanionState::Regrouping &&
		FVector::Dist(Companion->GetActorLocation(), Leader->GetActorLocation()) > Companion->GetLeashRadius())
	{
		Recall();
	}

	if (State == ECompanionState::Regrouping)
	{
		if (MoveToLeader())
		{
			StopMovement();
			SetState(ECompanionState::Idle);
		}

		Companion->SetInCombat(false);
		return;
	}

	GatherFight();
	Decide();
	Act();

	Companion->SetInCombat(bInFight);

	if (bDrawDecisionDebug)
	{
		FString Text = UEnum::GetDisplayValueAsText(State).ToString();

		if (Decision.IsSet() && Companion->GetTactics().IsValidIndex(Decision.TacticIndex))
		{
			Text += FString::Printf(TEXT("\n%s -> %s (%.2f)"),
				*Companion->GetTactics()[Decision.TacticIndex].AbilityTag.ToString(),
				*GetNameSafe(Decision.Target.Get()), Decision.Score);
		}

		if (const UCompanionNeedsComponent* Needs = Companion->GetNeedsComponent())
		{
			Text += TEXT("\n") + Needs->DescribeNeeds();
		}

		DrawDebugString(GetWorld(), FVector(0.0f, 0.0f, 130.0f), Text, Companion, FColor::Cyan, ThinkInterval, true);
	}
}

/* ============================== The fight ============================== */

void ACompanionAIController::GatherFight()
{
	const ACompanionCharacter* Companion = GetCompanion();
	const ABaseCharacter* Leader = GetLeader();
	UWorld* World = GetWorld();

	const double Now = GetWorldTime();
	const float AssistRadius = Companion->GetAssistRadius();
	const float AssistRadiusSquared = FMath::Square(AssistRadius);
	const FVector LeaderLocation = Leader->GetActorLocation();

	// Forget enemies that left the fight a while ago (or died).
	for (auto It = FightMemory.CreateIterator(); It; ++It)
	{
		if (!IsValidTarget(It.Key().Get()) || Now - It.Value() > Companion->GetAssistMemory())
		{
			It.RemoveCurrent();
		}
	}

	Candidates.Reset();

	const auto AddCandidate = [this, &LeaderLocation, AssistRadiusSquared](AActor* Hostile)
	{
		if (IsValidTarget(Hostile) &&
			FVector::DistSquared(Hostile->GetActorLocation(), LeaderLocation) <= AssistRadiusSquared)
		{
			Candidates.AddUnique(Hostile);
		}
	};

	// The leader's lock-on is part of the fight as soon as it's set.
	AddCandidate(GetLeaderTarget());

	for (const TPair<TWeakObjectPtr<AActor>, double>& Entry : FightMemory)
	{
		AddCandidate(Entry.Key.Get());
	}

	// Enemies near the leader: those already going for the leader or the wolf, or any it can see when it engages unprovoked.
	if (IsValid(World))
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CompanionFight), false, Companion);
		QueryParams.AddIgnoredActor(Leader);
		TArray<FOverlapResult> Overlaps;

		World->OverlapMultiByObjectType(Overlaps, LeaderLocation, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(AssistRadius), QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Hostile = Overlap.GetActor();

			if (!IsValidTarget(Hostile))
			{
				continue;
			}

			const bool bComingForUs = IsThreatening(Hostile, Leader, 0.0f) || IsThreatening(Hostile, Companion, 0.0f);

			if (bComingForUs || (Companion->EngagesUnprovoked() && LineOfSightTo(Hostile)))
			{
				AddCandidate(Hostile);
			}
		}
	}

	// The fight starts with its first enemy and ends a little after the last one is gone.
	if (!Candidates.IsEmpty())
	{
		if (!bInFight)
		{
			bInFight = true;
			FightStartTime = Now;
		}

		LastFightActivityTime = Now;
	}
	else if (bInFight && Now - LastFightActivityTime > FightEndDelay)
	{
		bInFight = false;
	}
}

bool ACompanionAIController::IsTacticAvailable(const int32 TacticIndex) const
{
	const ACompanionCharacter* Companion = GetCompanion();
	const FCompanionTactic& Tactic = Companion->GetTactics()[TacticIndex];
	const UAbilityComponent* Abilities = Companion->GetAbilityComponent();

	if (!Tactic.AbilityTag.IsValid() || !IsValid(Abilities))
	{
		return false;
	}

	const UAttributeComponent* Attributes = Companion->GetAttributeComponent();
	const int32 Level = IsValid(Attributes) ? Attributes->GetCharacterLevel() : 1;

	if (Level < Tactic.UnlockLevel)
	{
		return false;
	}

	const double Now = GetWorldTime();

	if (const double* BlockedUntil = TacticBlockedUntil.Find(TacticIndex); BlockedUntil && Now < *BlockedUntil)
	{
		return false;
	}

	if (const double* LastUsed = TacticLastUsed.Find(TacticIndex); LastUsed && Now - *LastUsed < Tactic.MinInterval)
	{
		return false;
	}

	// Granted, rank 1+, off cooldown.
	return Abilities->GetAbilityRank(Abilities->FindAbilityClassById(Tactic.AbilityTag)) > 0 &&
		!Abilities->IsAbilityOnCooldown(Tactic.AbilityTag);
}

float ACompanionAIController::ScoreTactic(const int32 TacticIndex, AActor* Target, const bool bLog) const
{
	const ACompanionCharacter* Companion = GetCompanion();
	const FCompanionTactic& Tactic = Companion->GetTactics()[TacticIndex];

	FCompanionTacticContext Context;
	Context.Brain = this;
	Context.Companion = Companion;
	Context.Leader = GetLeader();
	Context.Target = Target;
	Context.Gap = IsValid(Target) ? Companion->GetEdgeDistanceTo(Target) : 0.0f;

	if (Tactic.TargetType == ECompanionTacticTarget::Enemy)
	{
		if (Context.Gap < Tactic.MinRange || (!Tactic.bChaseIntoRange && Context.Gap > Tactic.MaxRange))
		{
			return 0.0f;
		}
	}

	float TacticScore = Tactic.BaseWeight;

	for (const UCompanionConsideration* Consideration : Tactic.Considerations)
	{
		if (IsValid(Consideration))
		{
			TacticScore *= FMath::Max(Consideration->Score(Context), 0.0f);
		}

		if (TacticScore <= 0.0f)
		{
			break;
		}
	}

	TacticScore *= GetPurposeMultiplier(Tactic.Purpose);

	if (Tactic.TargetType == ECompanionTacticTarget::Enemy)
	{
		// Nearer enemies are a little more attractive, and so is the one it's already on.
		const float Farness = FMath::Clamp(Context.Gap / FMath::Max(Companion->GetAssistRadius(), 1.0f), 0.0f, 1.0f);
		TacticScore *= 1.0f - DistancePenalty * Farness;

		if (Target == Decision.Target.Get())
		{
			TacticScore *= CurrentTargetBonus;
		}
	}

	if (bLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[Wolf]   %s on %s: %.2f (need x%.2f)"), *Tactic.AbilityTag.ToString(), *GetNameSafe(Target),
			TacticScore, GetPurposeMultiplier(Tactic.Purpose));
	}

	return TacticScore;
}

void ACompanionAIController::Decide()
{
	const ACompanionCharacter* Companion = GetCompanion();
	const TArray<FCompanionTactic>& Tactics = Companion->GetTactics();

	FCompanionDecision Best;
	float CurrentScore = 0.0f;
	const bool bLog = bLogScores && bInFight;

	if (bLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[Wolf] Deciding (%d enemies in the fight):"), Candidates.Num());
	}

	const auto Consider = [this, &Best, &CurrentScore, bLog](const int32 TacticIndex, AActor* Target)
	{
		const float CandidateScore = ScoreTactic(TacticIndex, Target, bLog);

		if (TacticIndex == Decision.TacticIndex && Target == Decision.Target.Get())
		{
			CurrentScore = CandidateScore;
		}

		if (CandidateScore >= MinScore && CandidateScore > Best.Score)
		{
			Best.TacticIndex = TacticIndex;
			Best.Target = Target;
			Best.Score = CandidateScore;
		}
	};

	for (int32 Index = 0; Index < Tactics.Num(); ++Index)
	{
		if (!IsTacticAvailable(Index))
		{
			continue;
		}

		switch (Tactics[Index].TargetType)
		{
		case ECompanionTacticTarget::Enemy:
			for (const TWeakObjectPtr<AActor>& Candidate : Candidates)
			{
				if (AActor* Enemy = Candidate.Get())
				{
					Consider(Index, Enemy);
				}
			}
			break;

		case ECompanionTacticTarget::Leader:
			Consider(Index, GetLeader());
			break;

		default:
			Consider(Index, nullptr);
			break;
		}
	}

	// Keep the current plan while it's still worth doing, unless the new one is clearly better.
	const bool bCurrentStillGood = Decision.IsSet() && CurrentScore >= MinScore;

	if (bCurrentStillGood && Best.Score <= CurrentScore * (1.0f + SwitchMargin))
	{
		Decision.Score = CurrentScore;
		return;
	}

	if (Best.TacticIndex != Decision.TacticIndex || Best.Target != Decision.Target)
	{
		LastChaseAcceptance = -1.0f;
	}

	Decision = Best;

	if (AActor* Target = Decision.Target.Get(); IsValid(Target) && Target != GetLeader())
	{
		SetFocus(Target);
	}
	else
	{
		ClearFocus(EAIFocusPriority::Gameplay);
	}
}

/* ============================== Doing it ============================== */

void ACompanionAIController::Act()
{
	ACompanionCharacter* Companion = GetCompanion();

	// Nothing worth doing: stay with the leader (guarding them when there's a fight).
	if (!Decision.IsSet())
	{
		if (MoveToLeader())
		{
			if (State == ECompanionState::Following || State == ECompanionState::Chasing)
			{
				StopMovement();
			}

			SetState(ECompanionState::Idle);
		}
		return;
	}

	const FCompanionTactic& Tactic = Companion->GetTactics()[Decision.TacticIndex];
	AActor* Target = Decision.Target.Get();

	if (Tactic.TargetType == ECompanionTacticTarget::Self)
	{
		if (GetMoveStatus() == EPathFollowingStatus::Moving)
		{
			StopMovement();
		}

		SetState(ECompanionState::Engaging);

		if (GetWorldTime() >= NextActionTime)
		{
			TryUseTactic(Decision.TacticIndex, nullptr);
		}
		return;
	}

	if (Tactic.TargetType == ECompanionTacticTarget::Leader)
	{
		if (!MoveToLeader(Tactic.MaxRange))
		{
			return;
		}

		SetState(ECompanionState::Engaging);

		if (GetWorldTime() >= NextActionTime)
		{
			TryUseTactic(Decision.TacticIndex, Target);
		}
		return;
	}

	// Enemy.
	if (!IsValidTarget(Target))
	{
		ClearDecision();
		return;
	}

	const float Gap = Companion->GetEdgeDistanceTo(Target);
	const bool bInRange = Gap <= Tactic.MaxRange;
	const bool bHasLineOfSight = !Tactic.bRequiresLineOfSight || LineOfSightTo(Target);

	// Out of range: close in. In range but blocked: walk in until the view is clear.
	if (!bInRange || !bHasLineOfSight)
	{
		const float ChaseAcceptance = bInRange ? LineOfSightChaseRadius : Tactic.MaxRange * ChaseRangeFraction;

		const bool bNeedsNewMove =
			State != ECompanionState::Chasing ||
			GetMoveStatus() != EPathFollowingStatus::Moving ||
			!FMath::IsNearlyEqual(ChaseAcceptance, LastChaseAcceptance);

		if (bNeedsNewMove)
		{
			MoveToActor(Target, ChaseAcceptance);
			LastChaseAcceptance = ChaseAcceptance;
			SetState(ECompanionState::Chasing);
		}
		return;
	}

	// On the run: keep after the target and strike without stopping.
	if (Tactic.bUseWhileMoving)
	{
		if (GetMoveStatus() != EPathFollowingStatus::Moving || State != ECompanionState::Chasing)
		{
			MoveToActor(Target, 0.0f);
			LastChaseAcceptance = 0.0f;
		}

		SetState(ECompanionState::Chasing);

		if (GetWorldTime() >= NextActionTime)
		{
			TryUseTactic(Decision.TacticIndex, Target);
		}
		return;
	}

	if (GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		StopMovement();
	}

	SetState(ECompanionState::Engaging);

	if (GetWorldTime() >= NextActionTime && IsFacing(Target))
	{
		TryUseTactic(Decision.TacticIndex, Target);
	}
}

bool ACompanionAIController::TryUseTactic(const int32 TacticIndex, AActor* Target)
{
	ACompanionCharacter* Companion = GetCompanion();
	UAbilityComponent* Abilities = Companion->GetAbilityComponent();
	const FCompanionTactic& Tactic = Companion->GetTactics()[TacticIndex];
	const float UsedScore = Decision.Score;

	if (!IsValid(Abilities) || !Abilities->TryActivateAbility(Tactic.AbilityTag))
	{
		// Couldn't start (no focus, blocked...): look elsewhere for a moment instead of trying every think.
		TacticBlockedUntil.Add(TacticIndex, GetWorldTime() + 1.0);
		ClearDecision();
		return false;
	}

	TacticLastUsed.Add(TacticIndex, GetWorldTime());
	SetState(ECompanionState::Acting);

	if (IsValidTarget(Target))
	{
		RememberInFight(Target);
	}

	OnCompanionAction.Broadcast(Tactic.AbilityTag, Target, UsedScore);

	// Decide afresh after this action, with the situation as it is then.
	Decision.TacticIndex = INDEX_NONE;
	return true;
}

bool ACompanionAIController::IsFacing(const AActor* Target) const
{
	const ACompanionCharacter* Companion = GetCompanion();
	const FVector Forward = Companion->GetActorForwardVector().GetSafeNormal2D();
	const FVector ToTarget = (Target->GetActorLocation() - Companion->GetActorLocation()).GetSafeNormal2D();

	return FVector::DotProduct(Forward, ToTarget) >= FMath::Cos(FMath::DegreesToRadians(Companion->MaxAttackAngle));
}

/* ============================== Following ============================== */

bool ACompanionAIController::MoveToLeader(const float AcceptDistance)
{
	const ACompanionCharacter* Companion = GetCompanion();
	ABaseCharacter* Leader = GetLeader();

	if (!IsValid(Companion) || !IsValid(Leader))
	{
		return true;
	}

	const float Distance = FVector::Dist(Companion->GetActorLocation(), Leader->GetActorLocation());

	if (Distance > Companion->GetTeleportDistance())
	{
		TeleportToLeader();
		return false;
	}

	const float FollowDistance = AcceptDistance >= 0.0f ? AcceptDistance : Companion->GetFollowDistance();

	// A little slack while idle, so it doesn't step every time the leader shifts.
	const float Slack = State == ECompanionState::Idle ? 1.25f : 1.0f;

	if (Distance <= FollowDistance * Slack)
	{
		return true;
	}

	if (State != ECompanionState::Following || GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		// Stops a bit inside follow distance, so it doesn't stop and start at the edge.
		const EPathFollowingRequestResult::Type Result = MoveToActor(Leader, FollowDistance * 0.6f);

		if (Result == EPathFollowingRequestResult::Failed)
		{
			// No path (a cliff, a river): catch up the magic way.
			TeleportToLeader();
			return false;
		}

		if (State != ECompanionState::Regrouping)
		{
			SetState(ECompanionState::Following);
		}
	}

	return false;
}

bool ACompanionAIController::TeleportToLeader()
{
	ACompanionCharacter* Companion = GetCompanion();
	const ABaseCharacter* Leader = GetLeader();

	if (!IsValid(Companion) || !IsValid(Leader))
	{
		return false;
	}

	// Behind the leader first (out of the camera's way), on ground at the leader's height.
	const FVector Ground = UCompanionComponent::FindPlacementNearLeader(Leader, -Leader->GetActorForwardVector(), Companion->GetFollowDistance());
	const FVector Destination = Ground + FVector(0.0f, 0.0f, Companion->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f);

	StopMovement();

	if (!Companion->TeleportTo(Destination, Leader->GetActorRotation()))
	{
		return false;
	}

	SetState(ECompanionState::Idle);
	return true;
}

/* ============================== Memory ============================== */

void ACompanionAIController::RememberInFight(AActor* Hostile)
{
	if (IsValidTarget(Hostile))
	{
		FightMemory.Add(Hostile, GetWorldTime());
	}
}

void ACompanionAIController::HandleLeaderHit(const FAbilityPayload& Payload, float DamageApplied)
{
	AActor* Attacker = Payload.Instigator.Get();

	if (IsValidTarget(Attacker))
	{
		LeaderAttackers.Add(Attacker, GetWorldTime());
		RememberInFight(Attacker);
	}
}

void ACompanionAIController::HandleLeaderDealtDamage(AActor* Target, const FAbilityPayload& Payload, float DamageApplied)
{
	RememberInFight(Target);
}

void ACompanionAIController::HandleSelfHit(const FAbilityPayload& Payload, float DamageApplied)
{
	AActor* Attacker = Payload.Instigator.Get();

	if (IsValidTarget(Attacker))
	{
		SelfAttackers.Add(Attacker, GetWorldTime());
		RememberInFight(Attacker);
	}
}

void ACompanionAIController::HandleSelfDealtDamage(AActor* Target, const FAbilityPayload& Payload, float DamageApplied)
{
	RememberInFight(Target);
}

void ACompanionAIController::HandleSelfDied(AActor* Killer)
{
	GetWorldTimerManager().ClearTimer(ThinkTimerHandle);
	ClearDecision();
	StopMovement();
	SetState(ECompanionState::Dead);
}

void ACompanionAIController::BindToLeader()
{
	const ABaseCharacter* Leader = GetLeader();
	UCombatantComponent* LeaderCombatant = IsValid(Leader) ? Leader->GetCombatantComponent() : nullptr;

	if (LeaderCombatant == BoundLeaderCombatant.Get())
	{
		return;
	}

	UnbindFromLeader();

	if (IsValid(LeaderCombatant))
	{
		LeaderCombatant->OnPayloadReceived.AddDynamic(this, &ACompanionAIController::HandleLeaderHit);
		LeaderCombatant->OnDamageDealt.AddDynamic(this, &ACompanionAIController::HandleLeaderDealtDamage);
		BoundLeaderCombatant = LeaderCombatant;
	}
}

void ACompanionAIController::UnbindFromLeader()
{
	if (UCombatantComponent* LeaderCombatant = BoundLeaderCombatant.Get())
	{
		LeaderCombatant->OnPayloadReceived.RemoveDynamic(this, &ACompanionAIController::HandleLeaderHit);
		LeaderCombatant->OnDamageDealt.RemoveDynamic(this, &ACompanionAIController::HandleLeaderDealtDamage);
	}

	BoundLeaderCombatant.Reset();
}

void ACompanionAIController::BindToSelf(ACompanionCharacter* Companion)
{
	UnbindFromSelf();

	if (UCombatantComponent* SelfCombatant = IsValid(Companion) ? Companion->GetCombatantComponent() : nullptr)
	{
		SelfCombatant->OnPayloadReceived.AddDynamic(this, &ACompanionAIController::HandleSelfHit);
		SelfCombatant->OnDamageDealt.AddDynamic(this, &ACompanionAIController::HandleSelfDealtDamage);
		SelfCombatant->OnDied.AddDynamic(this, &ACompanionAIController::HandleSelfDied);
		BoundSelfCombatant = SelfCombatant;
	}
}

void ACompanionAIController::UnbindFromSelf()
{
	if (UCombatantComponent* SelfCombatant = BoundSelfCombatant.Get())
	{
		SelfCombatant->OnPayloadReceived.RemoveDynamic(this, &ACompanionAIController::HandleSelfHit);
		SelfCombatant->OnDamageDealt.RemoveDynamic(this, &ACompanionAIController::HandleSelfDealtDamage);
		SelfCombatant->OnDied.RemoveDynamic(this, &ACompanionAIController::HandleSelfDied);
	}

	BoundSelfCombatant.Reset();
}
