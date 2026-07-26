#pragma once

#include "CoreMinimal.h"
#include "Ability.h"
#include "MontageAbility.generated.h"

class UAnimInstance;
class UAnimMontage;

/**
 * Reusable base for an ability whose execution is represented by a montage.
 *
 * UAbilityComponent does not know that this class uses animation.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UMontageAbility : public UAbility
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Transition")
	FGameplayTag OpenTransitionEventTag;

	/**
	 * Animation event that permanently closes the current execution's early
	 * cancellation window.
	 *
	 * Suggested tag:
	 * Event.Ability.EarlyCancellation.Close
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Early Cancellation")
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage", meta = (ClampMin = "0.0"))
	float EndBlendOutTime = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage", meta = (ClampMin = "0.0"))
	float EarlyCancellationBlendOutTime = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Montage")
	bool bEndAbilityWhenMontageEnds = true;

private:
	void HandleMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;
};