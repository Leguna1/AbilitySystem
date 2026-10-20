#pragma once

#include "CoreMinimal.h"
#include "ActiveAbility.h"
#include "MontageAbility.generated.h"

class UAnimInstance;
class UAnimMontage;
class UCombatantComponent;

/** When a montage variant is picked instead of the ability's default montage. */
UENUM(BlueprintType)
enum class EMontageVariantCondition : uint8
{
	/** The ability's target moves faster than Threshold (cm/s): a running bite, a chasing slash. */
	TargetMovingFasterThan UMETA(DisplayName = "Target Moving Faster Than"),

	/** The ability's target moves slower than Threshold (cm/s), or stands still. */
	TargetSlowerThan UMETA(DisplayName = "Target Slower Than"),

	/** The gap to the target (edge to edge) is more than Threshold (cm). */
	TargetFartherThan UMETA(DisplayName = "Target Farther Than"),

	/** The gap to the target (edge to edge) is less than Threshold (cm). */
	TargetCloserThan UMETA(DisplayName = "Target Closer Than"),

	/** The owner itself moves faster than Threshold (cm/s): attacking out of a run. */
	SelfMovingFasterThan UMETA(DisplayName = "Self Moving Faster Than"),

	/** There is no target. */
	NoTarget UMETA(DisplayName = "No Target")
};

/** "Play Montage instead when Condition holds." */
USTRUCT(BlueprintType)
struct FMontageVariant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage")
	EMontageVariantCondition Condition = EMontageVariantCondition::TargetMovingFasterThan;

	/** Speed (cm/s) or distance (cm), depending on Condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montage", meta = (ClampMin = "0.0"))
	float Threshold = 150.0f;
};

/**
 * Reusable base for an ability whose execution is represented by a montage.
 *
 * UAbilityComponent does not know that this class uses animation.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UMontageAbility : public UActiveAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;

	UFUNCTION(BlueprintPure, Category = "Ability|Montage")
	UAnimMontage* GetActiveMontage() const { return ActiveMontage; }

	UFUNCTION(BlueprintPure, Category = "Ability|Montage")
	bool IsAbilityMontagePlaying() const;
	
	virtual bool AcceptsAnimationEvent(const UAnimSequenceBase* SourceAnimation) const override;
	
	virtual void OnMovementInputReceived_Implementation(FVector2D MovementInput) override;
	
	/**
	 * Montages play faster or slower with the owner's attack speed, including attack speed scoped
	 * to this ability (e.g. "+20% for bow abilities"). Off for movement and reactions.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Animation")
	bool bAffectedByAttackSpeed = true;

	/** Attack speed gained or lost mid-montage changes the speed right away (stacking buffs, slows). Off = fixed at start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Animation", meta = (EditCondition = "bAffectedByAttackSpeed"))
	bool bUpdateAttackSpeedDuringMontage = true;

	/** This ability's attack speed right now: 1 = normal. Always 1 when not affected by attack speed. */
	UFUNCTION(BlueprintPure, Category = "Ability|Animation")
	float GetAttackSpeed() const;

protected:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Montage")
	UAnimMontage* SelectAbilityMontage() const;
	virtual UAnimMontage* SelectAbilityMontage_Implementation() const;

	/**
	 * Animation event that opens the ability's Transition window.
	 *
	 * Suggested tag:
	 * Event.Ability.Transition.Open
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage|Notify")
	FGameplayTag OpenTransitionEventTag;

	/**
	 * Animation event that permanently closes the current execution's early
	 * cancellation window.
	 *
	 * Suggested tag:
	 * Event.Ability.EarlyCancellation.Close
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage|Notify")
	FGameplayTag CloseEarlyCancellationEventTag;

	UFUNCTION(BlueprintCallable, Category = "Ability|Montage")
	bool PlayAbilityMontage(UAnimMontage* Montage, float PlayRate = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Ability|Montage")
	void StopAbilityMontage(float BlendOutTime = 0.0f);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Montage")
	void OnAbilityMontageStarted(UAnimMontage* Montage);
	virtual void OnAbilityMontageStarted_Implementation(UAnimMontage* Montage);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Montage")
	void OnAbilityMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	virtual void OnAbilityMontageBlendingOut_Implementation(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Montage")
	void OnAbilityMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	virtual void OnAbilityMontageEnded_Implementation(UAnimMontage* Montage, bool bInterrupted);

	UAnimInstance* GetAnimInstance() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage")
	TObjectPtr<UAnimMontage> AbilityMontage;

	/**
	 * Other montages for the same ability, picked by the situation when it starts: the first whose
	 * condition holds plays instead of Ability Montage. E.g. Bite: a running bite when the target moves.
	 * Abilities that pick their own montage (combos, rapid fire) ignore this.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage", meta = (TitleProperty = "Condition"))
	TArray<FMontageVariant> MontageVariants;

	/** Whether Variant's condition holds right now. */
	bool IsMontageVariantMet(const FMontageVariant& Variant) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage", meta = (ClampMin = "0.0"))
	float EndBlendOutTime = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage", meta = (ClampMin = "0.0"))
	float EarlyCancellationBlendOutTime = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage")
	bool bEndAbilityWhenMontageEnds = true;

	/* -------------------- Root motion distance override -------------------- */

	/**
	 * When true, the montage's root-motion translation is warped to travel exactly
	 * RootMotionDistance centimeters, regardless of the distance the animation
	 * authored. Requires a Motion Warping window on the montage using
	 * RootMotionWarpName. When false, the animation's own root motion is used.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Root Motion")
	bool bOverrideRootMotionDistance = false;

	/**
	 * Absolute travel distance in centimeters when bOverrideRootMotionDistance is
	 * true. 0 = in-place. 300 = 3m. 1000 = 10m. Independent of the animation.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Root Motion", meta = (ClampMin = "0.0", EditCondition = "bOverrideRootMotionDistance"))
	float RootMotionDistance = 300.0f;

	/**
	 * Travel to the ability's target instead of a fixed distance: a leap or lunge that lands on it.
	 * Stops Target Stop Distance short of the target's edge, travels at most Root Motion Distance.
	 * Without a target it falls back to Root Motion Distance straight ahead.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Root Motion", meta = (EditCondition = "bOverrideRootMotionDistance"))
	bool bWarpToAbilityTarget = false;

	/** Gap left between the two bodies when warping to the target (edge to edge). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Root Motion", meta = (ClampMin = "0.0", EditCondition = "bOverrideRootMotionDistance && bWarpToAbilityTarget"))
	float TargetStopDistance = 30.0f;

	/** Motion Warping window name on the montage that this override drives. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Root Motion", meta = (EditCondition = "bOverrideRootMotionDistance"))
	FName RootMotionWarpName = FName("RootMotionDistance");

	/**
	 * World-space direction the warped translation travels. Defaults to the
	 * character's forward at montage start; override for directional abilities
	 * (e.g. dodge returns its dodge direction).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Root Motion")
	FVector GetRootMotionWarpDirection() const;
	virtual FVector GetRootMotionWarpDirection_Implementation() const;

	/** Installs (or clears) the distance warp target for the given montage. Called on play. */
	void ApplyRootMotionDistanceWarp() const;
	
	/** From this event on, movement input ends the ability (recovery cancel). Empty = movement never cancels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage|Notify")
	FGameplayTag MovementCancelEventTag;

	/** Blend-out used when movement ends the ability. Short keeps it responsive. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Movement Cancel", meta = (ClampMin = "0.0"))
	float MovementCancelBlendOutTime = 0.12f;

	/** Movement input below this size never cancels (stick drift, tiny taps). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Movement Cancel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MovementCancelDeadZone = 0.3f;

private:
	void HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Buffs or slows changed: re-applies attack speed to the playing montage. */
	UFUNCTION()
	void HandleOwnerEffectsChanged();

	void BindAttackSpeedUpdates();
	void UnbindAttackSpeedUpdates();

	/** The rate the montage was asked to play at, before attack speed. */
	float BaseMontagePlayRate = 1.0f;

	/** Combatant whose effect changes update the play rate, while bound. */
	TWeakObjectPtr<UCombatantComponent> AttackSpeedSource;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;
	
	bool bMovementCancelOpen = false;
	bool bEndedByMovement = false;
};