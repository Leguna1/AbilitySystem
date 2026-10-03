#pragma once

#include "CoreMinimal.h"
#include "MontageAbility.h"
#include "ModifierTypes.h"
#include "OffensiveAbilityBase.generated.h"

/**
 * Common base for montage-driven offensive abilities.
 *
 * The shared Targeting and Motion Warping component references are cached by
 * UAbilityComponent during BeginPlay and inherited through UAbility.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UOffensiveAbilityBase : public UMontageAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;

protected:
	/**
	 * Configures the named Motion Warp target so the character faces the
	 * current automatic target.
	 *
	 * The montage's Motion Warping notify state determines whether translation
	 * or rotation is actually warped.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Offensive|Targeting")
	bool ConfigureTargetFacingWarp();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Offensive")
	void OnAttackStarted();
	virtual void OnAttackStarted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Offensive")
	void OnAttackFinished(EAbilityEndReason EndReason);
	virtual void OnAttackFinished_Implementation(EAbilityEndReason EndReason);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Offensive|Targeting")
	bool bUseTargetFacingWarp = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Offensive|Targeting", meta = (EditCondition = "bUseTargetFacingWarp"))
	FName TargetFacingWarpName = TEXT("AttackTarget");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Offensive|Targeting", meta = (EditCondition = "bUseTargetFacingWarp"))
	bool bRemoveTargetFacingWarpWhenFinished = true;
	
	UFUNCTION(BlueprintPure, Category = "Ability|Offensive|Weapon")
	UWeaponManagerComponent* GetWeaponManager() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Offensive|Weapon")
	AWeaponBase* GetEquippedWeapon() const;

	template <typename T>
	T* GetEquippedWeaponAs() const { return Cast<T>(GetEquippedWeapon()); }
	
	/** Statuses this ability's hits apply. Resolved at the ability's rank when it fires or strikes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Offensive|On Hit")
	TArray<FStatusEffectSpec> OnHitStatuses;

	/** OnHitStatuses whose attacker requirements are met, resolved at this execution's rank. */
	void ResolveOnHitStatuses(TArray<FStatusApplication>& OutStatuses) const;
};