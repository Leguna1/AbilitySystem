#pragma once

#include "CoreMinimal.h"
#include "OffensiveAbilityBase.h"
#include "TargetedSpellAbility.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * A spell that strikes its target directly. The cast montage sends the release event;
 * at that moment the target takes the hit if it's still within reach and in line of sight.
 * No projectile: the cast wind-up is the dodge window.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UTargetedSpellAbility : public UOffensiveAbilityBase
{
	GENERATED_BODY()

public:
	UTargetedSpellAbility();

	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;
	virtual FGameplayTag GetCostEventTag() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell")
	FAbilityRankedFloat DamageByRank = FAbilityRankedFloat(20.0f);

	/** Anim event on the cast montage where the spell lands. Required. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell")
	FGameplayTag ReleaseEventTag;

	/** If the target got farther than this by the release, the spell fizzles. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell", meta = (ClampMin = "0", Units = "cm"))
	float MaxReach = 2000.0f;

	/** A wall between caster and target at the release makes the spell fizzle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell")
	bool bRequireLineOfSight = true;

	/** Mesh socket the spell comes from (staff tip, hand). Missing = eye height. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell")
	FName CastSocketName = TEXT("hand_r");

	/** Spawned where the spell strikes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell|Visuals")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Spell|Visuals")
	TObjectPtr<USoundBase> ImpactSound;

	/**
	 * The spell resolved. bHit = the target took it; false = it fizzled (out of reach, behind a wall)
	 * or was refused (dodging, invulnerable). Add beams or extra effects here.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Spell")
	void OnSpellReleased(AActor* SpellTarget, FVector CastOrigin, FVector ImpactLocation, bool bHit);

	UFUNCTION(BlueprintPure, Category = "Ability|Spell")
	FVector GetCastOrigin() const;

private:
	void ReleaseSpell();
	bool CanReach(const AActor* SpellTarget, const FVector& FromLocation, const FVector& ToLocation) const;

	/** Taken at activation, so the spell finishes on whoever it was cast at. */
	TWeakObjectPtr<AActor> LockedTarget;

	float ResolvedDamage = 0.0f;
	bool bReleased = false;
};