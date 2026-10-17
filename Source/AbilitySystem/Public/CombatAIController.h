#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "AbilityTargetSource.h"
#include "CombatAIController.generated.h"

class ABaseAICharacter;
class UAbilityComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAITargetChangedSignature, AActor*, NewTarget);

UENUM(BlueprintType)
enum class EAIBehaviorState : uint8
{
	Idle,
	Roaming,
	Chasing,
	Engaging,      // In range, waiting for recovery or to face the target
	Attacking,
	ReturningHome,
	Following,
	Dead
};

/**
 * Shared brain of every AI fighter: one current target, and the fight against it.
 * Plans an attack from the character's Attacks list, chases into its range (and line of sight),
 * faces the target, attacks, recovers.
 *
 * Children decide who to fight and what to do otherwise:
 *   Enemy AI: sight and aggro, leash, roam and walk home.
 *   Companion AI: assist its leader, follow, regroup.
 */
UCLASS(Abstract)
class ABILITYSYSTEM_API ACombatAIController : public AAIController, public IAbilityTargetSource
{
	GENERATED_BODY()

public:
	/* -------------------- Target -------------------- */

	virtual AActor* GetAbilityTarget_Implementation() const override { return GetCurrentTarget(); }

	UFUNCTION(BlueprintPure, Category = "AI|Target")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "AI|Target")
	bool HasTarget() const { return CurrentTarget.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	void SetCurrentTarget(AActor* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	void ClearTarget();

	/** Someone hit this character. Enemies take them as a target; companions remember them. */
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	virtual void NotifyAttackedBy(AActor* Attacker);

	/**
	 * Use AbilityId now. An attack from the Attacks list is used on Target (chasing into range first,
	 * and Target becomes the current target). Anything else (a howl, a self buff) is used right away.
	 * False if it can't: not granted, on cooldown, or no valid target for an attack.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	virtual bool CommandAbility(FGameplayTag AbilityId, AActor* Target);

	/** Fight Target now, opening with the first Command Only attack that's ready (if any). */
	UFUNCTION(BlueprintCallable, Category = "AI|Target")
	virtual bool CommandAttack(AActor* Target);

	UPROPERTY(BlueprintAssignable, Category = "AI|Events")
	FAITargetChangedSignature OnTargetChanged;

	/* -------------------- Behavior -------------------- */

	UFUNCTION(BlueprintPure, Category = "AI|Behavior")
	EAIBehaviorState GetBehaviorState() const { return BehaviorState; }

	/** Alive, hostile, and not this character. */
	virtual bool IsValidTarget(const AActor* Actor) const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	/* -------------------- Hooks for children -------------------- */

	/** Every Evaluate Interval: keep, drop or find the target (sight, leash, assist...). */
	virtual void EvaluateTarget() {}

	/** Before fighting: return true when something else takes priority this think (walking home, regrouping). */
	virtual bool ThinkOverride() { return false; }

	/** No target: idle, roam, follow. */
	virtual void ThinkWithoutTarget() {}

	/** A new target was set. */
	virtual void OnTargetAcquired(AActor* NewTarget) {}

	/** Whether the nameplate should show "in combat". */
	virtual bool IsEngaged() const { return HasTarget(); }

	/** Extra debug drawing on top of the state text. */
	virtual void DrawBehaviorDebug() const {}

	/** Whether the AI may pick this attack on its own. Default: not Command Only. Companions let the player toggle it. */
	virtual bool IsAutoCastAttack(int32 AttackIndex) const;

	/** The current target was set by a command (not picked by the AI). Cleared with the target. */
	bool IsTargetCommanded() const { return bTargetCommanded; }

	/* -------------------- Settings -------------------- */

	/** How often the target is re-checked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Target", meta = (ClampMin = "0.1", Units = "s"))
	float EvaluateInterval = 0.5f;

	/** How often the character decides what to do. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Behavior", meta = (ClampMin = "0.05", Units = "s"))
	float ThinkInterval = 0.2f;

	/** Chase until this fraction of the attack's Max Range, so small target movement doesn't break range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Behavior", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ChaseRangeFraction = 0.8f;

	/** When an attack needs line of sight and there is none, walk in until this close (edge to edge) or until it's regained. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Behavior", meta = (ClampMin = "0.0", Units = "cm"))
	float LineOfSightChaseRadius = 100.0f;

	/** Shows the behavior state above the character's head. */
	UPROPERTY(EditAnywhere, Category = "AI|Debug")
	bool bDrawBehaviorDebug = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Behavior")
	EAIBehaviorState BehaviorState = EAIBehaviorState::Idle;

	/* -------------------- Shared helpers -------------------- */

	ABaseAICharacter* GetAICharacter() const;
	double GetWorldTime() const;
	void SetBehaviorState(EAIBehaviorState NewState) { BehaviorState = NewState; }

	/** Forgets the planned attack (and any commanded one). */
	void ResetAttackPlan();

private:
	void Think();
	void ThinkCombat(UAbilityComponent* Abilities, AActor* Target);
	void UpdateCombatState();

	int32 PickAttack(const UAbilityComponent* Abilities, float Gap) const;

	/** Off cooldown, far enough (Min Range), and either chasable or already in range. Command-only attacks need bAllowCommandOnly. */
	bool IsAttackUsable(const UAbilityComponent* Abilities, int32 AttackIndex, float Gap, bool bAllowCommandOnly = false) const;

	/** Highest Priority among usable attacks. False when none is usable. */
	bool FindHighestUsablePriority(const UAbilityComponent* Abilities, float Gap, int32& OutPriority) const;

	bool IsFacing(const AActor* Target) const;

	/** Granted with a rank above 0, and off cooldown. */
	static bool IsAbilityReady(const UAbilityComponent* Abilities, FGameplayTag AbilityId);

	TWeakObjectPtr<ABaseAICharacter> AICharacter;
	TWeakObjectPtr<AActor> CurrentTarget;

	int32 PlannedAttackIndex = INDEX_NONE;

	/** A commanded attack waiting to be used on the current target. */
	int32 CommandedAttackIndex = INDEX_NONE;

	bool bTargetCommanded = false;

	double NextAttackTime = 0.0;

	/** Acceptance radius of the current chase, so switching from "get in range" to "get line of sight" re-issues the move. */
	float LastChaseAcceptance = -1.0f;

	FTimerHandle EvaluateTimerHandle;
	FTimerHandle ThinkTimerHandle;
};
