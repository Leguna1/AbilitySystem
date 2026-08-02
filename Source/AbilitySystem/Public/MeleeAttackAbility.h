// MeleeAttackAbility.h
#pragma once

#include "CoreMinimal.h"
#include "OffensiveAbilityBase.h"
#include "GameplayTagContainer.h"
#include "MeleeAttackAbility.generated.h"

class ASwordBase;
class UWeaponComponent;

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
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;

protected:
	/** Damage delivered per hit this swing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee", meta = (ClampMin = "0.0"))
	float Damage = 25.0f;

	/** Anim event tag that starts the active (hit-detecting) window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee")
	FGameplayTag BeginHitWindowEventTag;

	/** Anim event tag that ends the active window. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee")
	FGameplayTag EndHitWindowEventTag;

	UFUNCTION()
	void HandleSwordHit(AActor* HitActor, const FHitResult& Hit);

private:
	UWeaponComponent* GetWeaponComponent() const;

	/** Bound to the sword during the active window so we can unbind cleanly. */
	UPROPERTY(Transient)
	TObjectPtr<ASwordBase> BoundSword;
};