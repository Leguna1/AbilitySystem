// MeleeAttackAbility.h
#pragma once

#include "CoreMinimal.h"
#include "OffensiveAbilityBase.h"
#include "GameplayTagContainer.h"
#include "ImpactGroupTypes.h"
#include "MeleeAttackAbility.generated.h"

class ASwordBase;

/**
 * A montage-driven melee attack. Sibling of URangedAttackAbility under
 * UOffensiveAbilityBase -- proves the offensive base is weapon-agnostic.
 *
 * Anim events toggle the equipped sword's hit detection for the active frames of
 * the swing. Sword overlaps are turned into IPayloadReceiver payloads here, so
 * damage attribution lives in the ability, not the weapon.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UMeleeAttackAbility : public UOffensiveAbilityBase
{
	GENERATED_BODY()
	

public:
	UMeleeAttackAbility();
	
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;
	virtual bool CanActivateAbility_Implementation() const override;
	
	virtual FGameplayTag GetCostEventTag() const override;

protected:
	/** Damage per hit, by rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee")
	FAbilityRankedFloat DamageByRank = FAbilityRankedFloat(25.0f);

	/** Anim event tag that starts the active (hit-detecting) window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee")
	FGameplayTag BeginHitWindowEventTag;

	/** Anim event tag that ends the active window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee")
	FGameplayTag EndHitWindowEventTag;

	UFUNCTION()
	void HandleSwordHit(AActor* HitActor, const FHitResult& Hit);
	
	/** One swing = one impact group. Leave sounds empty for silence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee|Impact Audio")
	FImpactGroupSettings SwingImpactGroup;
	
	/** Played per target the swing cuts (subject to SwingImpactGroup). Misses use the environment impact. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee|Impact Audio")
	FImpactFeedback SwingHitFeedback;

private:

	/** Bound to the sword during the active window so we can unbind cleanly. */
	UPROPERTY(Transient)
	TObjectPtr<ASwordBase> BoundSword;
	
	/** Ends detection on the bound sword and unbinds from its hits. */
	void ReleaseBoundSword();
	
	FImpactGroupHandle SwingGroup;
	
	UPROPERTY(Transient)
	float ResolvedDamage = 0.0f;
};