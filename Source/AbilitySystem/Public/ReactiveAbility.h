#pragma once

#include "CoreMinimal.h"
#include "PassiveAbility.h"
#include "PassiveTypes.h"
#include "ReactiveAbility.generated.h"

class UPassiveCondition;
class UPassiveEffect;
class UPassiveTrigger;

/**
 * A passive that reacts: WHEN any Trigger matches, IF every Condition passes, DO every Effect in order.
 *   e.g. On Gameplay Event (Event.Combo.Finished) + Chance 20%  ->  Buff Self (attack speed, 5 s)
 *        On Damage Dealt (Bull's Eye)                          ->  Apply Status (Crippled)
 *        On Health Below 30% + Max Triggers 1                  ->  Activate Ability (Enrage Roar)
 *
 * Always On modifiers still work here, for skills that need both ("+radius, and hits slow").
 * Anything no trigger, condition or effect covers: handle On Reaction Triggered in Blueprint.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UReactiveAbility : public UPassiveAbility
{
	GENERATED_BODY()

public:
	/* -------------------- Event hooks: each runs the reaction -------------------- */

	virtual void OnPassiveActivated_Implementation() override;
	virtual void OnPassiveDeactivated_Implementation() override;

	virtual void OnDamageDealt_Implementation(AActor* Target, const FAbilityPayload& Payload, float DamageApplied) override;
	virtual void OnPayloadReceived_Implementation(const FAbilityPayload& Payload, float DamageApplied) override;
	virtual void OnKilled_Implementation(AActor* Victim) override;
	virtual void OnDied_Implementation(AActor* Killer) override;
	virtual void OnHitReaction_Implementation(const FHitReactionResult& Reaction) override;
	virtual void OnHealthChanged_Implementation(float HealthBefore, float HealthAfter) override;
	virtual void OnAbilityActivated_Implementation(UActiveAbility* Ability) override;
	virtual void OnAbilityCommitted_Implementation(UActiveAbility* Ability) override;
	virtual void OnAbilityEnded_Implementation(UActiveAbility* Ability, EAbilityEndReason EndReason) override;
	virtual void OnGameplayEvent_Implementation(FGameplayTag EventTag, UActiveAbility* Source) override;

	/* -------------------- Used by effects and conditions -------------------- */

	/**
	 * Gives a timed effect charges: each committed ability with any of ConsumedByAbilityTags spends one,
	 * and the effect is removed when they run out. Shown as stacks.
	 */
	UFUNCTION(BlueprintCallable, Category = "Reaction")
	void SetEffectCharges(FName EntryKey, int32 Charges, const FGameplayTagContainer& ConsumedByAbilityTags);

	/** World time the reaction last fired. Very negative = never. */
	double GetLastTriggerTime() const { return LastTriggerTime; }

	/** Times the reaction fired since it was learned (or its rank changed). */
	int32 GetTriggerCount() const { return TriggerCount; }

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** The reaction fired: triggers matched, conditions passed, effects ran. For extra Blueprint behavior. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Reaction")
	void OnReactionTriggered(const FPassiveEventContext& Event);

	/** WHEN: fires if ANY of these matches. Click + and pick one, e.g. On Gameplay Event. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Reaction")
	TArray<TObjectPtr<UPassiveTrigger>> Triggers;

	/** IF: ALL of these must pass, checked in order (a failed one skips the rest). Empty = always. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Reaction")
	TArray<TObjectPtr<UPassiveCondition>> Conditions;

	/** DO: run in order when it fires. e.g. Buff Self, Apply Status. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Reaction")
	TArray<TObjectPtr<UPassiveEffect>> Effects;

private:
	struct FEffectCharges
	{
		FName EntryKey;
		int32 Remaining = 0;
		FGameplayTagContainer ConsumedByAbilityTags;
	};

	/** Runs the reaction for one event. */
	void HandleEvent(const FPassiveEventContext& Event);

	/** Spends charges of timed effects that Ability consumes. */
	void ConsumeEffectCharges(const UActiveAbility* Ability);

	/** Set while effects run, so an effect's own event (e.g. Send Gameplay Event) can't re-fire this reaction. */
	bool bHandlingEvent = false;

	double LastTriggerTime = -1.0e9;
	int32 TriggerCount = 0;

	TArray<FEffectCharges> ActiveCharges;
};
