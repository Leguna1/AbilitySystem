#pragma once

#include "CoreMinimal.h"
#include "Ability.h"
#include "ModifierTypes.h"
#include "PayloadReceiver.h"
#include "PassiveAbility.generated.h"

class UActiveAbility;

/**
 * An always-on ability. Granted like any ability; one persistent instance
 * exists while granted. Active while its rank is 1+: applies its modifiers
 * and granted tags, and receives combat and ability events.
 *
 * Pure stat passives need no code (fill Modifiers). Reactive passives
 * override the event hooks and use ApplyTimedEffect for procs.
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

protected:
	/**
	 * Applies (or refreshes) a timed effect owned by this passive, at its rank.
	 * EntryKey must not be None (reserved for the passive's permanent entry).
	 */
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ApplyTimedEffect(FName EntryKey, const TArray<FStatModifier>& EffectModifiers, const FGameplayTagContainer& EffectTags, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Passive")
	void RemoveTimedEffect(FName EntryKey);

	/** Applied for as long as the passive is active, at its rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive")
	TArray<FStatModifier> Modifiers;

	/** Owner tags granted for as long as the passive is active. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive")
	FGameplayTagContainer GrantedTags;
	
	UFUNCTION(BlueprintPure, Category = "Passive")
	bool HasTimedEffect(FName EntryKey) const;
	
	/** Sets the UI stack count of one of this passive's timed effects. */
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void SetTimedEffectStacks(FName EntryKey, int32 Stacks);

private:
	friend class UAbilityComponent;

	void ActivatePassive();
	void DeactivatePassive();

	bool bPassiveActive = false;
	
	
};