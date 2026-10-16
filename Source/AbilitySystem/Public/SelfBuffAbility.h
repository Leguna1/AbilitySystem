#pragma once

#include "CoreMinimal.h"
#include "MontageAbility.h"
#include "ModifierTypes.h"
#include "SelfBuffAbility.generated.h"

/**
 * Plays a cast montage and buffs its user: war cries, stances, Hawk Eye.
 *
 * Each entry in Buffs is a status on yourself, so it shows on the effects bar with its name and icon,
 * scales with this ability's rank, and casting again refreshes it instead of stacking.
 * Description tokens come from the modifiers as usual ({AttackSpeed}, {Duration}...).
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API USelfBuffAbility : public UMontageAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityCommitted_Implementation() override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** The buffs. Status Tag identifies each (e.g. Status.Buff.HawkEye); Display is what the effects bar shows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Buff", meta = (TitleProperty = "StatusTag"))
	TArray<FStatusEffectSpec> Buffs;

	/**
	 * Animation event at which the buffs land (the fist raised, the roar). Empty = as soon as the ability commits.
	 * Interrupted before the event = no buff.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Buff")
	FGameplayTag ApplyBuffEventTag;

	/** Applies the buffs now. Called once per cast; override to add extras (a heal, a sound). */
	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Buff")
	void ApplyBuffs();
	virtual void ApplyBuffs_Implementation();

	/** The buffs were applied. For Blueprint extras: a Niagara burst, a camera shake. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Buff")
	void OnBuffsApplied();

private:
	bool bBuffsApplied = false;
};
