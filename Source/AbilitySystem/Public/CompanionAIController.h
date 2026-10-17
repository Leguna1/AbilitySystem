#pragma once

#include "CoreMinimal.h"
#include "CombatAIController.h"
#include "PayloadReceiver.h"
#include "CompanionAIController.generated.h"

class ABaseCharacter;
class ACompanionCharacter;
class UCombatantComponent;

/** How a companion picks fights, like a WoW pet's stance. */
UENUM(BlueprintType)
enum class ECompanionStance : uint8
{
	/** Attacks your target, and any hostile it sees near you. */
	Aggressive,

	/** Joins fights you're in: enemies that hit you or it, or that you hit. */
	Defensive,

	/** Never starts a fight: follows, and only attacks when commanded. */
	Passive
};

/**
 * Runs a companion. The fight comes from Combat AI Controller; this decides who to fight, by stance:
 *   Aggressive: the leader's target, else the nearest hostile it can see within Assist Radius of the leader.
 *   Defensive:  the nearest hostile that recently hit, or was hit by, the leader or the companion.
 *   Passive:    only commanded targets.
 * Commands (the companion bar) always work. Only auto-cast attacks are used on its own.
 * Out of combat it follows the leader; too far behind, it drops the fight and regroups (or teleports).
 */
UCLASS()
class ABILITYSYSTEM_API ACompanionAIController : public ACombatAIController
{
	GENERATED_BODY()

public:
	virtual void NotifyAttackedBy(AActor* Attacker) override;
	virtual bool CommandAbility(FGameplayTag AbilityId, AActor* Target) override;
	virtual bool CommandAttack(AActor* Target) override;

	UFUNCTION(BlueprintCallable, Category = "Companion")
	void SetStance(ECompanionStance NewStance);

	UFUNCTION(BlueprintPure, Category = "Companion")
	ECompanionStance GetStance() const { return Stance; }

	/**
	 * Whether the companion uses AbilityId on its own. Attacks start from Command Only; other abilities
	 * (a Howl) start off and, when switched on, are used whenever ready during a fight.
	 */
	UFUNCTION(BlueprintPure, Category = "Companion")
	bool IsAutoCast(FGameplayTag AbilityId) const;

	UFUNCTION(BlueprintCallable, Category = "Companion")
	void SetAutoCast(FGameplayTag AbilityId, bool bEnabled);

	/** Drop the fight and come back to the leader. */
	UFUNCTION(BlueprintCallable, Category = "Companion")
	void Recall();

	UFUNCTION(BlueprintPure, Category = "Companion")
	bool IsRegrouping() const { return bRegrouping; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	virtual void EvaluateTarget() override;
	virtual bool ThinkOverride() override;
	virtual void ThinkWithoutTarget() override;
	virtual bool IsEngaged() const override { return HasTarget() && !bRegrouping; }
	virtual bool IsAutoCastAttack(int32 AttackIndex) const override;

private:
	UFUNCTION()
	void HandleLeaderHit(const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandleLeaderDealtDamage(AActor* Target, const FAbilityPayload& Payload, float DamageApplied);

	/** Binds to the leader's combat events (the leader can be set after possession). */
	void BindToLeader();
	void UnbindFromLeader();

	void RememberHostile(AActor* Hostile);

	/** Who to fight next, by stance. Null = nobody. */
	AActor* FindStanceTarget() const;
	AActor* FindRecentHostile() const;
	AActor* FindLeaderTarget() const;
	AActor* FindVisibleHostileNearLeader() const;

	/** In a fight: uses one ready, auto-cast ability that isn't an attack (a buff). True if it used one. */
	bool TryAutoCastSupportAbility();

	/** Runs to the leader. True once within Follow Distance. Teleports when too far or stuck. */
	bool MoveToLeader();
	bool TeleportToLeader();

	ACompanionCharacter* GetCompanion() const;
	ABaseCharacter* GetLeader() const;

	/** Hostiles in the fight, and when they were last part of it. */
	TMap<TWeakObjectPtr<AActor>, double> RecentHostiles;

	TWeakObjectPtr<UCombatantComponent> BoundLeaderCombatant;

	bool bRegrouping = false;

	ECompanionStance Stance = ECompanionStance::Defensive;

	/** Player's auto-cast choices; abilities not listed use their Command Only default. */
	TMap<FGameplayTag, bool> AutoCastOverrides;
};
