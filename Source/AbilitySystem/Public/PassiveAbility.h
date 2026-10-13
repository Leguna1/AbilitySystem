#pragma once

#include "CoreMinimal.h"
#include "Ability.h"
#include "HitReactionTypes.h"
#include "ModifierTypes.h"
#include "PayloadReceiver.h"
#include "PassiveAbility.generated.h"

class UActiveAbility;
class UCombatantComponent;

/**
 * An always-on ability: stats and tags for as long as it's learned (rank 1+).
 * Granted like any ability; one persistent instance exists while granted.
 * Most skill tree upgrades are just this: fill Modifiers, no code.
 *
 * For "when X happens, do Y", use Reactive Ability (a child of this class).
 * The event hooks below are for custom C++ or Blueprint passives; they do nothing by default.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UPassiveAbility : public UAbility
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Passive")
	bool IsPassiveActive() const { return bPassiveActive; }

	/* -------------------- Lifecycle -------------------- */

	/** Modifiers and granted tags are already applied when this runs. */
	UFUNCTION(BlueprintNativeEvent, Category = "Passive")
	void OnPassiveActivated();
	virtual void OnPassiveActivated_Implementation();

	/** Runs before modifiers, tags and timed effects are removed. */
	UFUNCTION(BlueprintNativeEvent, Category = "Passive")
	void OnPassiveDeactivated();
	virtual void OnPassiveDeactivated_Implementation();

	/* -------------------- Combat events -------------------- */

	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Combat")
	void OnDamageDealt(AActor* Target, const FAbilityPayload& Payload, float DamageApplied);
	virtual void OnDamageDealt_Implementation(AActor* Target, const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Combat")
	void OnPayloadReceived(const FAbilityPayload& Payload, float DamageApplied);
	virtual void OnPayloadReceived_Implementation(const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Combat")
	void OnKilled(AActor* Victim);
	virtual void OnKilled_Implementation(AActor* Victim);

	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Combat")
	void OnDied(AActor* Killer);
	virtual void OnDied_Implementation(AActor* Killer);

	/** A hit made the owner react (flinch or stronger). */
	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Combat")
	void OnHitReaction(const FHitReactionResult& Reaction);
	virtual void OnHitReaction_Implementation(const FHitReactionResult& Reaction);

	/** Health changed. Values are fractions of max health (0..1). */
	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Combat")
	void OnHealthChanged(float HealthBefore, float HealthAfter);
	virtual void OnHealthChanged_Implementation(float HealthBefore, float HealthAfter);

	/* -------------------- Ability events -------------------- */

	/** Runs before the ability's own activation, so effects applied here still reach its snapshot. */
	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Abilities")
	void OnAbilityActivated(UActiveAbility* Ability);
	virtual void OnAbilityActivated_Implementation(UActiveAbility* Ability);

	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Abilities")
	void OnAbilityCommitted(UActiveAbility* Ability);
	virtual void OnAbilityCommitted_Implementation(UActiveAbility* Ability);

	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Abilities")
	void OnAbilityEnded(UActiveAbility* Ability, EAbilityEndReason EndReason);
	virtual void OnAbilityEnded_Implementation(UActiveAbility* Ability, EAbilityEndReason EndReason);

	/** A gameplay event sent by an active ability (e.g. Event.Combo.Finished). */
	UFUNCTION(BlueprintNativeEvent, Category = "Passive|Abilities")
	void OnGameplayEvent(FGameplayTag EventTag, UActiveAbility* Source);
	virtual void OnGameplayEvent_Implementation(FGameplayTag EventTag, UActiveAbility* Source);

	/* -------------------- Timed effects -------------------- */

	/**
	 * Applies (or refreshes) a timed effect owned by this passive, at its rank.
	 * EntryKey must not be None (reserved for the permanent entry). Duration 0 = until removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ApplyTimedEffect(FName EntryKey, const TArray<FStatModifier>& EffectModifiers, const FGameplayTagContainer& EffectTags, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Passive")
	void RemoveTimedEffect(FName EntryKey);

	UFUNCTION(BlueprintPure, Category = "Passive")
	bool HasTimedEffect(FName EntryKey) const;

	/** Sets the UI stack count of one of this passive's timed effects. */
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void SetTimedEffectStacks(FName EntryKey, int32 Stacks);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** Applied for as long as the passive is learned, at its rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Always On")
	TArray<FStatModifier> Modifiers;

	/** Owner tags granted for as long as the passive is learned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Always On")
	FGameplayTagContainer GrantedTags;

private:
	friend class UAbilityComponent;

	void ActivatePassive();
	void DeactivatePassive();

	bool bPassiveActive = false;

	/** Where this passive's effects live: the owner's combatant component. */
	UCombatantComponent* GetEffectTarget() const;
};
