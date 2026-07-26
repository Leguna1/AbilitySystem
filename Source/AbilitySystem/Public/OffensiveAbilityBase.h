#pragma once

#include "CoreMinimal.h"
#include "MontageAbility.h"
#include "OffensiveAbilityBase.generated.h"

/**
 * Common base for montage-driven offensive abilities.
 *
 * It defines attack lifecycle hooks without assuming whether the attack is
 * melee, ranged, charged, repeated, or combo-based.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UOffensiveAbilityBase : public UMontageAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;

protected:
	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Offensive")
	void OnAttackStarted();
	virtual void OnAttackStarted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Offensive")
	void OnAttackFinished(EAbilityEndReason EndReason);
	virtual void OnAttackFinished_Implementation(EAbilityEndReason EndReason);
};