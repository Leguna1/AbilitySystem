// MeleeAttackAbility.h
#pragma once

#include "CoreMinimal.h"
#include "OffensiveAbilityBase.h"
#include "GameplayTagContainer.h"
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
class UMeleeTraceComponent;

UENUM(BlueprintType)
enum class EMeleeHitSource : uint8
{
	/** The equipped sword's hit box, opened by the hit-window events. */
	EquippedWeapon UMETA(DisplayName = "Equipped Weapon"),

	/** The owner's Melee Trace Component, opened by Melee Trace notifies on the montage. */
	Body UMETA(DisplayName = "Body (Melee Trace)")
};

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
	
	/** What lands the hits: the equipped weapon, or body parts (fists, feet) through Melee Trace notifies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee")
	EMeleeHitSource HitSource = EMeleeHitSource::EquippedWeapon;
	
	/** Damage per hit, by rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee")
	FAbilityRankedFloat DamageByRank = FAbilityRankedFloat(25.0f);

	/** Anim event tag that starts the active (hit-detecting) window. Weapon attacks only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee",
		meta = (EditCondition = "HitSource == EMeleeHitSource::EquippedWeapon", EditConditionHides))
	FGameplayTag BeginHitWindowEventTag;

	/** Anim event tag that ends the active window. Weapon attacks only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Melee",
		meta = (EditCondition = "HitSource == EMeleeHitSource::EquippedWeapon", EditConditionHides))
	FGameplayTag EndHitWindowEventTag;
	
	

	UFUNCTION()
	void HandleSwordHit(AActor* HitActor, const FHitResult& Hit);

private:

	/** Bound to the sword during the active window so we can unbind cleanly. */
	UPROPERTY(Transient)
	TObjectPtr<ASwordBase> BoundSword;
	
	/** Ends detection on the bound sword and unbinds from its hits. */
	void ReleaseBoundSword();
	
	UPROPERTY(Transient)
	float ResolvedDamage = 0.0f;
	
	/** Turns one hit into a payload and impact feedback, whatever landed it. */
	void ProcessMeleeHit(AActor* HitActor, const FHitResult& Hit, AActor* Causer);

	/**
	 * Hit feedback for body attacks (fists, bites): Cue.Weapon.Hit / Miss from the Feedback
	 * component on the attacker, or the project default set if it has none.
	 */
	void PlayBodyHitFeedback(AActor* HitActor, const FHitResult& Hit, const FVector& StrikeLocation, bool bHitTarget, AActor* Causer) const;

	void BindBodyTrace();
	void ReleaseBodyTrace();

	UFUNCTION()
	void HandleBodyTraceStarted(UAnimSequenceBase* SourceAnimation);

	UFUNCTION()
	void HandleBodyHit(AActor* HitActor, const FHitResult& Hit, FName ShapeName, UAnimSequenceBase* SourceAnimation);

	UPROPERTY(Transient)
	TObjectPtr<UMeleeTraceComponent> BodyTrace;
};