#pragma once

#include "CoreMinimal.h"
#include "PassiveAbility.h"
#include "OnHitStatusPassive.generated.h"

/**
 * While learned, hits from abilities with any of TriggerAbilityTags apply
 * Statuses to the target, at this passive's rank. A status sharing a tag
 * with one the ability already applies replaces it (e.g. mark variants).
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UOnHitStatusPassive : public UPassiveAbility
{
	GENERATED_BODY()

public:
	virtual void OnDamageDealt_Implementation(AActor* Target, const FAbilityPayload& Payload, float DamageApplied) override;
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** Hits from abilities with any of these tags trigger it (e.g. Ability.Bow.BullsEye). Empty = every hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "On Hit")
	FGameplayTagContainer TriggerAbilityTags;

	/** Chance per hit (0..1), by this passive's rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "On Hit")
	FAbilityRankedFloat ChanceByRank = FAbilityRankedFloat(1.0f);

	/** Applied to the target. Magnitudes and durations are read at this passive's rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "On Hit")
	TArray<FStatusEffectSpec> Statuses;
};