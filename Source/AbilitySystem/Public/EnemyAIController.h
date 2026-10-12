#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "AbilityTargetSource.h"
#include "EnemyAIController.generated.h"

class ABaseEnemy;
class UAbilityComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyTargetChangedSignature, AActor*, NewTarget);

UENUM(BlueprintType)
enum class EEnemyBehaviorState : uint8
{
	Idle,
	Roaming,
	Chasing,
	Engaging,      // In range, waiting for recovery or to face the target
	Attacking,
	ReturningHome,
	Dead
};

/**
 * Runs an enemy.
 * Targeting: sees hostiles through AI Perception, keeps one current target,
 * forgets it when unseen for a while, and gives up when dragged too far from home.
 * Behavior: plans an attack from the enemy's Attacks list, chases into its range,
 * faces the target, attacks, recovers, and walks home after leashing.
 */
UCLASS()
class ABILITYSYSTEM_API AEnemyAIController : public AAIController, public IAbilityTargetSource
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	/* -------------------- Target -------------------- */
	
	virtual AActor* GetAbilityTarget_Implementation() const override { return GetCurrentTarget(); }

	UFUNCTION(BlueprintPure, Category = "Enemy|Target")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Enemy|Target")
	bool HasTarget() const { return CurrentTarget.IsValid(); }

	/** True after leashing, until the enemy is back near home. No new targets meanwhile. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Target")
	bool IsReturningHome() const { return bReturningHome; }

	UFUNCTION(BlueprintCallable, Category = "Enemy|Target")
	void SetCurrentTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Target")
	void ClearTarget();

	/** Being hit by a hostile makes it the target, if there isn't one yet. Also used by packs to alert members. */
	UFUNCTION(BlueprintCallable, Category = "Enemy|Target")
	void NotifyAttackedBy(AActor* Attacker);

	UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
	FEnemyTargetChangedSignature OnTargetChanged;

	/* -------------------- Behavior -------------------- */

	UFUNCTION(BlueprintPure, Category = "Enemy|Behavior")
	EEnemyBehaviorState GetBehaviorState() const { return BehaviorState; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Perception")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Seconds a target can stay out of sight before it's forgotten. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Target", meta = (ClampMin = "0.0", Units = "s"))
	float ForgetTargetAfter = 5.0f;

	/** How often the target is re-checked (leash, forgetting, death). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Target", meta = (ClampMin = "0.1", Units = "s"))
	float EvaluateInterval = 0.5f;

	/** Within this distance of home, a returning enemy may pick targets again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Target", meta = (ClampMin = "0.0", Units = "cm"))
	float HomeReachedDistance = 200.0f;

	/** How often the enemy decides what to do (chase, attack, return). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Behavior", meta = (ClampMin = "0.05", Units = "s"))
	float ThinkInterval = 0.2f;

	/** Chase until this fraction of the attack's Max Range, so small target movement doesn't break range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Behavior", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ChaseRangeFraction = 0.8f;

	/** Shows the behavior state above the enemy's head. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Debug")
	bool bDrawBehaviorDebug = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Enemy|Behavior")
	EEnemyBehaviorState BehaviorState = EEnemyBehaviorState::Idle;
	
	/** When an attack needs line of sight and there is none, walk in until this close (edge to edge) or until it's regained. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Behavior", meta = (ClampMin = "0.0", Units = "cm"))
	float LineOfSightChaseRadius = 100.0f;

private:
	/* -------------------- Target -------------------- */

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void EvaluateTarget();
	bool IsValidTarget(const AActor* Actor) const;
	AActor* FindNearestSeenTarget() const;
	AActor* FindNearestTargetInProximity() const;
	double GetWorldTime() const;
	
	/** Out of combat, an enemy farther than this from home walks back. The roam area, at least HomeReachedDistance. */
	float GetHomeAreaRadius() const;

	/* -------------------- Behavior -------------------- */

	void Think();
	void ThinkCombat(UAbilityComponent* Abilities, AActor* Target);
	void ThinkReturnHome();
	int32 PickAttack(const UAbilityComponent* Abilities, float Gap) const;
	/** Off cooldown, far enough (Min Range), and either chasable or already in range. */
	bool IsAttackUsable(const UAbilityComponent* Abilities, int32 AttackIndex, float Gap) const;

	/** Highest Priority among usable attacks. False when none is usable. */
	bool FindHighestUsablePriority(const UAbilityComponent* Abilities, float Gap, int32& OutPriority) const;
	
	bool IsFacing(const AActor* Target) const;
	void SetBehaviorState(EEnemyBehaviorState NewState);
	
	void ThinkIdle();
	void StartIdleWait();
	bool FindRoamPoint(FVector& OutPoint) const;
	
	/** Sets bReturningHome and keeps State.Invulnerable on the enemy in sync with it. */
	void SetReturningHome(bool bNewReturningHome);

	/* -------------------- State -------------------- */

	TWeakObjectPtr<ABaseEnemy> Enemy;
	TWeakObjectPtr<AActor> CurrentTarget;

	double LastSeenTime = 0.0;
	bool bReturningHome = false;

	int32 PlannedAttackIndex = INDEX_NONE;
	double NextAttackTime = 0.0;

	FTimerHandle EvaluateTimerHandle;
	FTimerHandle ThinkTimerHandle;
	double NextRoamTime = 0.0;
	
	/** Acceptance radius of the current chase, so switching from "get in range" to "get line of sight" re-issues the move. */
	float LastChaseAcceptance = -1.0f;
	
};