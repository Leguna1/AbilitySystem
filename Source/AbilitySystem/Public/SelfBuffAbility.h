#pragma once

#include "CoreMinimal.h"
#include "MontageAbility.h"
#include "ModifierTypes.h"
#include "SelfBuffAbility.generated.h"

/**
 * Plays a cast montage and buffs its user: war cries, stances, Hawk Eye.
 * With Ally Radius above 0, friendly characters that close get the same buffs: a wolf's Howl buffs
 * the wolf and its leader (and any other ally nearby).
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

	/** Allies (same team, alive) within this distance get the buffs too. 0 = only the user. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Buff", meta = (ClampMin = "0", Units = "cm"))
	float AllyRadius = 0.0f;

	/** Off = allies only (an aura for the party, not the caster). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Buff", meta = (EditCondition = "AllyRadius > 0"))
	bool bBuffSelf = true;

	/** Applies the buffs now. Called once per cast; override to add extras (a heal, a sound). */
	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Buff")
	void ApplyBuffs();
	virtual void ApplyBuffs_Implementation();

	/** The buffs were applied. For Blueprint extras: a Niagara burst, a camera shake. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Buff")
	void OnBuffsApplied();

private:
	/** The user (if Buff Self) and every living ally within Ally Radius. */
	void GatherBuffTargets(TArray<class UCombatantComponent*>& OutTargets) const;

	bool bBuffsApplied = false;
};
