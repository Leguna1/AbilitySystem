#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AbilityTargetSource.h"
#include "CompanionTactics.h"
#include "GameplayTagContainer.h"
#include "PayloadReceiver.h"
#include "CompanionAIController.generated.h"

class ABaseCharacter;
class ACompanionCharacter;
class UAbilityComponent;
class UCombatantComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCompanionActionSignature, FGameplayTag, AbilityId, AActor*, Target, float, Score);

/** What the wolf is doing, for debugging and animation. */
UENUM(BlueprintType)
enum class ECompanionState : uint8
{
	Idle,
	Following,
	Chasing,
	Engaging,
	Acting,
	Regrouping,
	Dead
};

/** The tactic the wolf has settled on: which one, on whom, and how good it looked. */
USTRUCT(BlueprintType)
struct FCompanionDecision
{
	GENERATED_BODY()

	/** Index into the companion's Tactics. INDEX_NONE = nothing chosen. */
	UPROPERTY(BlueprintReadOnly, Category = "Companion")
	int32 TacticIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Companion")
	TWeakObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadOnly, Category = "Companion")
	float Score = 0.0f;

	bool IsSet() const { return TacticIndex != INDEX_NONE; }
	void Reset() { *this = FCompanionDecision(); }
};

/**
 * The wolf's own mind. No stances and no commands: it decides for itself.
 *
 * Every think it gathers the enemies in the fight (ones going for the leader or for it, ones the leader
 * hits or targets), scores every ready tactic against every one of them (see FCompanionTactic), and does
 * the best. It sticks with its choice unless something clearly better comes up (Switch Margin).
 * Out of a fight it follows the leader; too far away it drops everything and regroups or teleports.
 *
 * Its own system: it doesn't share the enemy AI's code, so either can change freely.
 */
UCLASS()
class ABILITYSYSTEM_API ACompanionAIController : public AAIController, public IAbilityTargetSource
{
	GENERATED_BODY()

public:
	ACompanionAIController();

	/** Abilities aim at the target of what the wolf is doing now. */
	virtual AActor* GetAbilityTarget_Implementation() const override;

	/** Drop the fight and come back to the leader (Summon pressed while the wolf is out). */
	UFUNCTION(BlueprintCallable, Category = "Companion")
	void Recall();

	UFUNCTION(BlueprintPure, Category = "Companion")
	bool IsRegrouping() const { return State == ECompanionState::Regrouping; }

	UFUNCTION(BlueprintPure, Category = "Companion")
	ECompanionState GetCompanionState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Companion")
	FCompanionDecision GetCurrentDecision() const { return Decision; }

	/** An enemy is part of the current fight. */
	UFUNCTION(BlueprintPure, Category = "Companion")
	bool IsInFight() const { return bInFight; }

	/** The wolf used one of its abilities: good for barks, a growl, a howl call. */
	UPROPERTY(BlueprintAssignable, Category = "Companion")
	FCompanionActionSignature OnCompanionAction;

	/* -------------------- Queries for considerations -------------------- */

	ACompanionCharacter* GetCompanion() const;
	ABaseCharacter* GetLeader() const;

	/** The leader's lock-on target, if it's a living enemy. */
	AActor* GetLeaderTarget() const;

	/** Alive, hostile to the wolf's team. */
	bool IsValidTarget(const AActor* Actor) const;

	/** Living enemies within Radius of Center. */
	int32 CountHostilesNear(const FVector& Center, float Radius) const;

	/** Hostile hit Victim in the last Within seconds, or is targeting Victim right now. */
	bool IsThreatening(const AActor* Hostile, const AActor* Victim, float Within) const;

	/** The enemies in the current fight (this think's). */
	TArray<AActor*> GetFightEnemies() const;

	double GetFightStartTime() const { return FightStartTime; }
	double GetWorldTime() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/**
	 * How much the moment favors a purpose: the companion's Needs Component (what the leader needs now).
	 * Later: also its learned habits. 1 = neutral. Multiplies every tactic with that purpose.
	 */
	virtual float GetPurposeMultiplier(ECompanionPurpose Purpose) const;

	/** How often the wolf decides what to do. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Brain", meta = (ClampMin = "0.05", Units = "s"))
	float ThinkInterval = 0.2f;

	/** Below this score a tactic isn't worth doing. With nothing worth doing, the wolf stays with the leader. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Brain", meta = (ClampMin = "0.0"))
	float MinScore = 0.05f;

	/** It changes its mind only for something this much better (0.25 = 25%), so it doesn't flip between choices. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Brain", meta = (ClampMin = "0.0"))
	float SwitchMargin = 0.25f;

	/** Bonus for staying on the enemy it's already fighting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Brain", meta = (ClampMin = "1.0"))
	float CurrentTargetBonus = 1.15f;

	/** How much farther enemies are disliked: 0.3 = an enemy at Assist Radius scores 30% less than one next to it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Brain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistancePenalty = 0.3f;

	/** Seconds without any enemy in the fight before it counts as over. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Brain", meta = (ClampMin = "0.0", Units = "s"))
	float FightEndDelay = 2.0f;

	/** Chase until this fraction of the tactic's Max Range, so small target movement doesn't break range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Movement", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ChaseRangeFraction = 0.8f;

	/** In range but no line of sight: walk in until this close (edge to edge) or until it's clear. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Movement", meta = (ClampMin = "0.0", Units = "cm"))
	float LineOfSightChaseRadius = 100.0f;

	/** Shows what it's doing and why above its head. */
	UPROPERTY(EditAnywhere, Category = "Companion|Debug")
	bool bDrawDecisionDebug = false;

	/** Logs every tactic's score whenever it decides (LogTemp, [Wolf]). Noisy. */
	UPROPERTY(EditAnywhere, Category = "Companion|Debug")
	bool bLogScores = false;

private:
	void Think();

	void GatherFight();
	void Decide();
	void Act();

	/** Score of one tactic on one target. 0 = not usable or not a good idea. */
	float ScoreTactic(int32 TacticIndex, AActor* Target, bool bLog) const;

	/** Unlocked, granted, ready, past its Min Interval, and not just failed. */
	bool IsTacticAvailable(int32 TacticIndex) const;

	bool IsFacing(const AActor* Target) const;
	bool TryUseTactic(int32 TacticIndex, AActor* Target);

	/** Runs to the leader. True once within follow distance (or Max Distance if given). Teleports when too far or stuck. */
	bool MoveToLeader(float AcceptDistance = -1.0f);
	bool TeleportToLeader();

	void SetState(ECompanionState NewState) { State = NewState; }
	void ClearDecision();

	/* -------------------- Memory -------------------- */

	UFUNCTION()
	void HandleLeaderHit(const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandleLeaderDealtDamage(AActor* Target, const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandleSelfHit(const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandleSelfDealtDamage(AActor* Target, const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandleSelfDied(AActor* Killer);

	void BindToLeader();
	void UnbindFromLeader();
	void BindToSelf(ACompanionCharacter* Companion);
	void UnbindFromSelf();

	void RememberInFight(AActor* Hostile);

	TWeakObjectPtr<UCombatantComponent> BoundLeaderCombatant;
	TWeakObjectPtr<UCombatantComponent> BoundSelfCombatant;

	/** Enemies in the fight and when they last took part (hit or were hit by the leader or the wolf). */
	TMap<TWeakObjectPtr<AActor>, double> FightMemory;

	/** When each enemy last hit the leader / the wolf. */
	TMap<TWeakObjectPtr<AActor>, double> LeaderAttackers;
	TMap<TWeakObjectPtr<AActor>, double> SelfAttackers;

	/** This think's enemies to choose from. */
	TArray<TWeakObjectPtr<AActor>> Candidates;

	FCompanionDecision Decision;

	/** When each tactic was last used, and until when one that failed to start is skipped. */
	TMap<int32, double> TacticLastUsed;
	TMap<int32, double> TacticBlockedUntil;

	ECompanionState State = ECompanionState::Idle;

	bool bInFight = false;
	double FightStartTime = 0.0;
	double LastFightActivityTime = 0.0;

	double NextActionTime = 0.0;
	float LastChaseAcceptance = -1.0f;

	FTimerHandle ThinkTimerHandle;
};
