#include "CompanionAIController.h"

#include "AbilityComponent.h"
#include "BaseAICharacter.h"
#include "CombatantComponent.h"
#include "CompanionCharacter.h"
#include "CompanionComponent.h"
#include "Engine/OverlapResult.h"
#include "TargetingComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

void ACompanionAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	BindToLeader();
}

void ACompanionAIController::OnUnPossess()
{
	UnbindFromLeader();
	RecentHostiles.Reset();

	Super::OnUnPossess();
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

/* ============================== Leader events ============================== */

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

void ACompanionAIController::HandleLeaderHit(const FAbilityPayload& Payload, float DamageApplied)
{
	RememberHostile(Payload.Instigator.Get());
}

void ACompanionAIController::HandleLeaderDealtDamage(AActor* Target, const FAbilityPayload& Payload, float DamageApplied)
{
	RememberHostile(Target);
}

void ACompanionAIController::NotifyAttackedBy(AActor* Attacker)
{
	RememberHostile(Attacker);
}

void ACompanionAIController::RememberHostile(AActor* Hostile)
{
	if (IsValidTarget(Hostile))
	{
		RecentHostiles.Add(Hostile, GetWorldTime());
	}
}

/* ============================== Commands ============================== */

bool ACompanionAIController::CommandAbility(const FGameplayTag AbilityId, AActor* Target)
{
	// A command overrides regrouping and stance: the player decided.
	if (!Super::CommandAbility(AbilityId, Target))
	{
		return false;
	}

	bRegrouping = false;
	RememberHostile(Target);
	return true;
}

bool ACompanionAIController::CommandAttack(AActor* Target)
{
	if (!Super::CommandAttack(Target))
	{
		return false;
	}

	bRegrouping = false;
	RememberHostile(Target);
	return true;
}

void ACompanionAIController::Recall()
{
	ClearTarget();
	RecentHostiles.Reset();
	bRegrouping = true;
}

void ACompanionAIController::SetStance(const ECompanionStance NewStance)
{
	Stance = NewStance;

	// Passive stops fights it picked itself at once; a commanded fight goes on.
	if (Stance == ECompanionStance::Passive && HasTarget() && !IsTargetCommanded())
	{
		ClearTarget();
	}
}

bool ACompanionAIController::IsAutoCast(const FGameplayTag AbilityId) const
{
	if (const bool* Override = AutoCastOverrides.Find(AbilityId))
	{
		return *Override;
	}

	const ABaseAICharacter* Fighter = GetAICharacter();

	if (!IsValid(Fighter))
	{
		return false;
	}

	const FAIAttack* Attack = Fighter->Attacks.FindByPredicate([AbilityId](const FAIAttack& Candidate)
	{
		return Candidate.AbilityTag.MatchesTagExact(AbilityId);
	});

	return Attack && !Attack->bCommandOnly;
}

void ACompanionAIController::SetAutoCast(const FGameplayTag AbilityId, const bool bEnabled)
{
	if (AbilityId.IsValid())
	{
		AutoCastOverrides.Add(AbilityId, bEnabled);
	}
}

bool ACompanionAIController::IsAutoCastAttack(const int32 AttackIndex) const
{
	const ABaseAICharacter* Fighter = GetAICharacter();
	return IsValid(Fighter) && Fighter->Attacks.IsValidIndex(AttackIndex) && IsAutoCast(Fighter->Attacks[AttackIndex].AbilityTag);
}

/* ============================== Target ============================== */

void ACompanionAIController::EvaluateTarget()
{
	BindToLeader();

	const ACompanionCharacter* Companion = GetCompanion();
	const ABaseCharacter* Leader = GetLeader();

	if (!IsValid(Companion) || Companion->IsDead() || !IsValid(Leader) || Leader->IsDead())
	{
		ClearTarget();
		return;
	}

	// Forget hostiles that left the fight a while ago (or died).
	const double Now = GetWorldTime();
	const double Memory = Companion->GetAssistMemory();

	for (auto It = RecentHostiles.CreateIterator(); It; ++It)
	{
		if (!IsValidTarget(It.Key().Get()) || Now - It.Value() > Memory)
		{
			It.RemoveCurrent();
		}
	}

	// Dragged too far from the leader: drop it and come back.
	if (FVector::Dist(Companion->GetActorLocation(), Leader->GetActorLocation()) > Companion->GetLeashRadius())
	{
		if (HasTarget())
		{
			Recall();
		}
		return;
	}

	if (bRegrouping)
	{
		return;
	}

	if (AActor* Target = GetCurrentTarget())
	{
		if (IsValidTarget(Target))
		{
			return;
		}

		ClearTarget();
	}

	if (AActor* NewTarget = FindStanceTarget())
	{
		SetCurrentTarget(NewTarget);
	}
}

AActor* ACompanionAIController::FindStanceTarget() const
{
	switch (Stance)
	{
	case ECompanionStance::Aggressive:
		if (AActor* LeaderTarget = FindLeaderTarget())
		{
			return LeaderTarget;
		}

		if (AActor* Recent = FindRecentHostile())
		{
			return Recent;
		}

		return FindVisibleHostileNearLeader();

	case ECompanionStance::Defensive:
		return FindRecentHostile();

	default:
		return nullptr;
	}
}

AActor* ACompanionAIController::FindLeaderTarget() const
{
	const ABaseCharacter* Leader = GetLeader();
	const ACompanionCharacter* Companion = GetCompanion();
	const UTargetingComponent* Targeting = IsValid(Leader) ? Leader->FindComponentByClass<UTargetingComponent>() : nullptr;
	AActor* LeaderTarget = IsValid(Targeting) ? Targeting->GetCurrentTarget() : nullptr;

	if (!IsValidTarget(LeaderTarget) ||
		FVector::Dist(LeaderTarget->GetActorLocation(), Leader->GetActorLocation()) > Companion->GetAssistRadius())
	{
		return nullptr;
	}

	return LeaderTarget;
}

AActor* ACompanionAIController::FindVisibleHostileNearLeader() const
{
	const ABaseCharacter* Leader = GetLeader();
	const ACompanionCharacter* Companion = GetCompanion();
	UWorld* World = GetWorld();

	if (!IsValid(Leader) || !IsValid(Companion) || !IsValid(World))
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CompanionAggro), false, Companion);
	QueryParams.AddIgnoredActor(Leader);

	TArray<FOverlapResult> Overlaps;

	World->OverlapMultiByObjectType(
		Overlaps,
		Leader->GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Companion->GetAssistRadius()),
		QueryParams
	);

	AActor* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Hostile = Overlap.GetActor();

		if (!IsValidTarget(Hostile) || !LineOfSightTo(Hostile))
		{
			continue;
		}

		const float Distance = FVector::DistSquared(Hostile->GetActorLocation(), Companion->GetActorLocation());

		if (Distance < NearestDistance)
		{
			Nearest = Hostile;
			NearestDistance = Distance;
		}
	}

	return Nearest;
}

AActor* ACompanionAIController::FindRecentHostile() const
{
	const ACompanionCharacter* Companion = GetCompanion();
	const ABaseCharacter* Leader = GetLeader();

	AActor* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	const float AssistRadiusSquared = FMath::Square(Companion->GetAssistRadius());

	for (const TPair<TWeakObjectPtr<AActor>, double>& Entry : RecentHostiles)
	{
		AActor* Hostile = Entry.Key.Get();

		if (!IsValidTarget(Hostile) ||
			FVector::DistSquared(Hostile->GetActorLocation(), Leader->GetActorLocation()) > AssistRadiusSquared)
		{
			continue;
		}

		const float Distance = FVector::DistSquared(Hostile->GetActorLocation(), Companion->GetActorLocation());

		if (Distance < NearestDistance)
		{
			Nearest = Hostile;
			NearestDistance = Distance;
		}
	}

	return Nearest;
}

/* ============================== Behavior ============================== */

bool ACompanionAIController::ThinkOverride()
{
	if (!bRegrouping)
	{
		// Buffs first in a fight: a howl before the next bite.
		return HasTarget() && TryAutoCastSupportAbility();
	}

	ResetAttackPlan();

	if (MoveToLeader())
	{
		bRegrouping = false;
		StopMovement();
		SetBehaviorState(EAIBehaviorState::Idle);
	}

	return true;
}

void ACompanionAIController::ThinkWithoutTarget()
{
	ResetAttackPlan();

	if (MoveToLeader())
	{
		if (BehaviorState == EAIBehaviorState::Following)
		{
			StopMovement();
		}

		SetBehaviorState(EAIBehaviorState::Idle);
	}
}

bool ACompanionAIController::TryAutoCastSupportAbility()
{
	const ABaseAICharacter* Fighter = GetAICharacter();
	UAbilityComponent* Abilities = IsValid(Fighter) ? Fighter->GetAbilityComponent() : nullptr;

	if (!IsValid(Abilities) || AutoCastOverrides.IsEmpty())
	{
		return false;
	}

	for (const TPair<FGameplayTag, bool>& Choice : AutoCastOverrides)
	{
		const FGameplayTag AbilityId = Choice.Key;

		// Attacks are the fight's own business; only switched-on, ready non-attacks here.
		const bool bIsAttack = Fighter->Attacks.ContainsByPredicate([AbilityId](const FAIAttack& Attack)
		{
			return Attack.AbilityTag.MatchesTagExact(AbilityId);
		});

		if (!Choice.Value || bIsAttack || !IsAbilityReady(Abilities, AbilityId))
		{
			continue;
		}

		if (Abilities->TryActivateAbility(AbilityId))
		{
			return true;
		}
	}

	return false;
}

bool ACompanionAIController::MoveToLeader()
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

	// Within follow distance: there. Slight slack while already idle, so it doesn't step every time the leader shifts.
	const float Slack = BehaviorState == EAIBehaviorState::Idle ? 1.25f : 1.0f;

	if (Distance <= Companion->GetFollowDistance() * Slack)
	{
		return true;
	}

	if (BehaviorState != EAIBehaviorState::Following || GetMoveStatus() != EPathFollowingStatus::Moving)
	{
		// Stops a bit inside follow distance, so it doesn't stop and start at the edge.
		const EPathFollowingRequestResult::Type Result = MoveToActor(Leader, Companion->GetFollowDistance() * 0.6f);

		if (Result == EPathFollowingRequestResult::Failed)
		{
			// No path (a cliff, a river): catch up the magic way.
			TeleportToLeader();
			return false;
		}

		SetBehaviorState(EAIBehaviorState::Following);
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

	// Behind the leader first (out of the way of the camera), on ground at the leader's height.
	const FVector Ground = UCompanionComponent::FindPlacementNearLeader(Leader, -Leader->GetActorForwardVector(), Companion->GetFollowDistance());
	const FVector Destination = Ground + FVector(0.0f, 0.0f, Companion->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f);

	StopMovement();

	if (!Companion->TeleportTo(Destination, Leader->GetActorRotation()))
	{
		return false;
	}

	SetBehaviorState(EAIBehaviorState::Idle);
	return true;
}
