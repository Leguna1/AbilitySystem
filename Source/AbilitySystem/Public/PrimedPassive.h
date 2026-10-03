#pragma once

#include "CoreMinimal.h"
#include "PassiveAbility.h"
#include "PrimedPassive.generated.h"

/**
 * On TriggerEventTag, primes the next N matching skills (N = ChargesByRank):
 * BonusModifiers apply until N skills with ConsumingAbilityTags have committed,
 * or until DurationByRank runs out. Triggering again refills the charges.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UPrimedPassive : public UPassiveAbility
{
	GENERATED_BODY()

public:
	virtual void OnGameplayEvent_Implementation(FGameplayTag EventTag, UActiveAbility* Source) override;
	virtual void OnAbilityCommitted_Implementation(UActiveAbility* Ability) override;
	virtual void OnPassiveDeactivated_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Primed")
	int32 GetRemainingCharges() const { return RemainingCharges; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primed")
	FGameplayTag TriggerEventTag;

	/** How many skill uses the bonus lasts, by rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primed")
	FAbilityRankedInt ChargesByRank = FAbilityRankedInt(1);

	/** The bonus while primed. Scope it to the skills that should benefit (e.g. Ability.Type.Special). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primed")
	TArray<FStatModifier> BonusModifiers;

	/** Committing an ability with any of these tags spends one charge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primed")
	FGameplayTagContainer ConsumingAbilityTags;

	/** Owner tags while primed (e.g. State.Primed), for UI and other rules. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primed")
	FGameplayTagContainer PrimedTags;

	/** Seconds before unused charges expire, by rank. 0 = until used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Primed")
	FAbilityRankedFloat DurationByRank = FAbilityRankedFloat(10.0f);

private:
	int32 RemainingCharges = 0;
};