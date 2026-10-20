#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CompanionTactics.h"
#include "PayloadReceiver.h"
#include "CompanionNeedsComponent.generated.h"

class ABaseCharacter;
class ACompanionAIController;
class UCombatantComponent;

/**
 * How the companion reads the moment: what its leader needs right now, per purpose, from 0 to 1.
 *
 *   Protect  enemies in melee range of the leader, damage the leader just took, low health
 *   Damage   the leader is focusing one enemy (locked on for a while, a channel like Rapid Fire, a Marked target)
 *   Control  several enemies near the leader, or closing in on them
 *   Support  a fight just started, the leader is low on health or focus
 *   Finish   an enemy in the fight is low or poise-broken
 *
 * Needs rise quickly and fade slowly, so the companion reacts fast but doesn't flicker.
 * The companion's brain multiplies every tactic by its purpose's multiplier (see Need Influence).
 * Lives on the companion; reads its leader.
 */
UCLASS(ClassGroup = (Companion), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UCompanionNeedsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionNeedsComponent();

	/** The smoothed need for Purpose, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Companion|Needs")
	float GetNeed(ECompanionPurpose Purpose) const;

	/** What the brain multiplies tactics of Purpose by: 1 + Need Influence x (2 x Need - 1). */
	UFUNCTION(BlueprintPure, Category = "Companion|Needs")
	float GetNeedMultiplier(ECompanionPurpose Purpose) const;

	/** The purpose with the highest need right now (for barks and debugging). */
	UFUNCTION(BlueprintPure, Category = "Companion|Needs")
	ECompanionPurpose GetStrongestNeed() const;

	/** One line per need, for on-screen debugging. */
	FString DescribeNeeds() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * How much needs sway the companion's choices. 0 = not at all (only its tactics' own considerations).
	 * 0.5 = a need of 1 makes matching tactics 1.5x as attractive, a need of 0 makes them 0.5x.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float NeedInfluence = 0.5f;

	/** Seconds between readings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs", meta = (ClampMin = "0.05", Units = "s"))
	float UpdateInterval = 0.25f;

	/** How fast a need climbs toward a higher reading, per second (1 = from 0 to full in a second). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs", meta = (ClampMin = "0.1"))
	float RiseRate = 4.0f;

	/** How fast a need fades toward a lower reading, per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs", meta = (ClampMin = "0.05"))
	float FallRate = 0.5f;

	/* -------------------- Protect -------------------- */

	/** Enemies this close to the leader count as "on them". */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Protect", meta = (ClampMin = "0", Units = "cm"))
	float MeleeRadius = 350.0f;

	/** Each enemy on the leader adds this much Protect. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Protect", meta = (ClampMin = "0.0"))
	float ProtectPerMeleeEnemy = 0.4f;

	/** Damage taken in the last Recent Damage Window seconds, as a fraction of max health, counts this many times over. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Protect", meta = (ClampMin = "0.0"))
	float RecentDamageWeight = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Protect", meta = (ClampMin = "0.5", Units = "s"))
	float RecentDamageWindow = 3.0f;

	/* -------------------- Damage (assist) -------------------- */

	/** Locked on to the same enemy this long = full Damage need. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Damage", meta = (ClampMin = "0.1", Units = "s"))
	float FocusBuildTime = 3.0f;

	/** The leader's lock-on target has any of these: Damage need +0.3 (focus fire on marks). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Damage")
	FGameplayTagContainer FocusTargetTags;

	/** The leader is using any of these abilities (Rapid Fire, Charged Shot): Damage need +0.3. Empty = none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Damage", meta = (Categories = "Ability"))
	FGameplayTagContainer FocusAbilityTags;

	/* -------------------- Control -------------------- */

	/** Enemies within this distance of the leader count toward Control. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Control", meta = (ClampMin = "0", Units = "cm"))
	float ControlRadius = 900.0f;

	/** This many enemies around the leader (closing in counts half) = full Control need. One enemy = none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Control", meta = (ClampMin = "2"))
	int32 CrowdSize = 4;

	/* -------------------- Support -------------------- */

	/** For this long after a fight starts, Support is high (time for an opening howl), fading to none. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Support", meta = (ClampMin = "0", Units = "s"))
	float FightOpeningTime = 5.0f;

	/** Below this health fraction the leader needs Support, rising to full at 0. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Support", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthFraction = 0.5f;

	/** Below this focus fraction the leader needs Support too (at half weight). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Support", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowFocusFraction = 0.25f;

	/* -------------------- Finish -------------------- */

	/** An enemy in the fight below this health fraction is a finishing chance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Finish", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FinishHealthFraction = 0.3f;

	/** A poise-broken enemy in the fight (State.Staggered) is a finishing chance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Finish")
	bool bPoiseBrokenIsFinish = true;

	/** An enemy in the fight with any of these is a finishing chance too (e.g. Status.Pinned). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Needs|Finish")
	FGameplayTagContainer FinishTargetTags;

private:
	void UpdateNeeds();

	float ReadProtect(const ACompanionAIController& Brain, const ABaseCharacter& Leader) const;
	float ReadDamage(const ACompanionAIController& Brain, const ABaseCharacter& Leader);
	float ReadControl(const ACompanionAIController& Brain, const ABaseCharacter& Leader) const;
	float ReadSupport(const ACompanionAIController& Brain, const ABaseCharacter& Leader) const;
	float ReadFinish(const ACompanionAIController& Brain) const;

	UFUNCTION()
	void HandleLeaderHit(const FAbilityPayload& Payload, float DamageApplied);

	void BindToLeader(ABaseCharacter* Leader);
	void UnbindFromLeader();

	ACompanionAIController* GetBrain() const;
	double GetWorldTime() const;

	/** Smoothed needs, indexed by ECompanionPurpose. */
	TArray<float> Needs;

	/** Damage the leader took, and when. */
	TArray<TPair<double, float>> RecentLeaderDamage;

	/** The leader's lock-on target and since when. */
	TWeakObjectPtr<AActor> FocusTarget;
	double FocusSince = 0.0;

	TWeakObjectPtr<UCombatantComponent> BoundLeaderCombatant;

	FTimerHandle UpdateTimerHandle;
	double LastUpdateTime = 0.0;
};
