#pragma once

#include "CoreMinimal.h"
#include "RangedAttackAbility.h"
#include "RangedComboAttackAbility.generated.h"

class UAnimMontage;

/**
 * One ranged ability execution containing multiple projectile attack steps.
 *
 * Each montage represents one combo step and contains:
 *
 * PrepareProjectile event
 * ReleaseProjectile event
 * InputCheckPoint event
 * optional Transition.Open event
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API URangedComboAttackAbility : public URangedAttackAbility
{
	GENERATED_BODY()

public:
	URangedComboAttackAbility();

	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;
	virtual bool CanHandleRepeatedActivationRequest_Implementation() const override;
	virtual bool HandleRepeatedActivationRequest_Implementation() override;
	virtual void OnAbilityMontageEnded_Implementation(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged Combo")
	int32 GetCurrentComboIndex() const { return CurrentComboIndex; }

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged Combo")
	int32 GetComboStepCount() const { return ComboMontages.Num(); }

protected:
	virtual UAnimMontage* SelectAbilityMontage_Implementation() const override;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged Combo")
	void OnComboStepStarted(int32 ComboIndex);
	virtual void OnComboStepStarted_Implementation(int32 ComboIndex);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged Combo")
	void OnComboStepFinished(int32 ComboIndex);
	virtual void OnComboStepFinished_Implementation(int32 ComboIndex);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged Combo")
	void OnComboAdvanced(int32 PreviousComboIndex, int32 NewComboIndex);
	virtual void OnComboAdvanced_Implementation(int32 PreviousComboIndex, int32 NewComboIndex);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged Combo")
	TArray<TObjectPtr<UAnimMontage>> ComboMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged Combo")
	bool bLoopCombo = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged Combo|Events")
	FGameplayTag InputCheckPointEventTag;

private:
	bool PlayCurrentComboStep();
	bool IsFinalComboStep() const;

	UPROPERTY(Transient)
	int32 CurrentComboIndex = 0;

	UPROPERTY(Transient)
	bool bChangingComboStep = false;
};