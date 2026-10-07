#pragma once

#include "CoreMinimal.h"
#include "PassiveAbility.h"
#include "ChanceBuffPassive.generated.h"

/**
 * On TriggerEventTag, rolls ChanceByRank; on success applies BuffModifiers and
 * BuffTags for DurationByRank seconds. A proc while active refreshes the buff.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UChanceBuffPassive : public UPassiveAbility
{
	GENERATED_BODY()

public:
	virtual void OnGameplayEvent_Implementation(FGameplayTag EventTag, UActiveAbility* Source) override;
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** Event that rolls for the buff, e.g. Event.Combo.Finished. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chance Buff")
	FGameplayTag TriggerEventTag;

	/** Chance per trigger (0..1), by rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chance Buff")
	FAbilityRankedFloat ChanceByRank = FAbilityRankedFloat(0.2f);

	/** Buff duration in seconds, by rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chance Buff")
	FAbilityRankedFloat DurationByRank = FAbilityRankedFloat(5.0f);

	/** Applied while the buff runs. Magnitudes are read at this passive's rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chance Buff")
	TArray<FStatModifier> BuffModifiers;

	/** Owner tags while the buff runs (e.g. State.Buff.QuickHands), for UI and other rules. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Chance Buff")
	FGameplayTagContainer BuffTags;
};