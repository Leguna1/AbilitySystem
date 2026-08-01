#pragma once

#include "CoreMinimal.h"
#include "RangedAttackAbility.h"
#include "AbilityProgressProvider.h"
#include "ChargedShotAbility.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EChargedShotStage : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Windup UMETA(DisplayName = "Windup"),
	Charging UMETA(DisplayName = "Charging"),
	Recovery UMETA(DisplayName = "Recovery")
};

UENUM(BlueprintType)
enum class EChargeInterruptionResponse : uint8
{
	Ignore UMETA(DisplayName = "Ignore"),
	Cancel UMETA(DisplayName = "Cancel"),
	Release UMETA(DisplayName = "Release")
};

/**
 * Configurable charged or channeled projectile attack.
 *
 * Standard automatic charged shot:
 * - Auto Release At Maximum Charge = true
 * - Movement Interruption Response = Cancel
 *
 * Held/channelled shot released by movement:
 * - Auto Release At Maximum Charge = false
 * - Movement Interruption Response = Release
 *
 * Uninterruptible channel:
 * - Movement Interruption Response = Ignore
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UChargedShotAbility : public URangedAttackAbility, public IAbilityProgressProvider
{
	GENERATED_BODY()

public:
	UChargedShotAbility();

	virtual FAbilityProgress GetAbilityProgress_Implementation() const override;

	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void TickAbility_Implementation(float DeltaTime) override;
	virtual void OnMovementInputReceived_Implementation(FVector2D MovementInput) override;
	virtual void OnAbilityMontageEnded_Implementation(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;
	virtual bool CanReplaceActiveAbility_Implementation(const UAbility* CurrentAbility) const override;

	UFUNCTION(BlueprintPure, Category = "Ability|Charged Shot")
	EChargedShotStage GetChargedShotStage() const { return ChargedShotStage; }

	UFUNCTION(BlueprintPure, Category = "Ability|Charged Shot")
	float GetCurrentChargeTime() const { return CurrentChargeTime; }

	UFUNCTION(BlueprintPure, Category = "Ability|Charged Shot")
	float GetMaximumChargeTime() const { return MaximumChargeTime; }

	UFUNCTION(BlueprintPure, Category = "Ability|Charged Shot")
	float GetNormalizedCharge() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Charged Shot")
	bool HasReachedMaximumCharge() const { return bMaximumChargeReached; }

protected:
	virtual float ResolveProjectileStrength_Implementation() const override;

	/**
	 * Commits the current accumulated charge and enters Recovery.
	 *
	 * Immediate:
	 * - jumps directly to Recovery.
	 *
	 * Non-immediate:
	 * - finishes the current Charge iteration first.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Charged Shot")
	bool RequestChargedRelease(bool bImmediate);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Charged Shot")
	void OnWindupStarted();
	virtual void OnWindupStarted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Charged Shot")
	void OnChargeStarted();
	virtual void OnChargeStarted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Charged Shot")
	void OnChargeUpdated(float ChargeTime, float MaximumTime, float NormalizedCharge);
	virtual void OnChargeUpdated_Implementation(float ChargeTime, float MaximumTime, float NormalizedCharge);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Charged Shot")
	void OnMaximumChargeReached();
	virtual void OnMaximumChargeReached_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Charged Shot")
	void OnChargeReleaseRequested(bool bImmediate);
	virtual void OnChargeReleaseRequested_Implementation(bool bImmediate);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Charged Shot")
	void OnRecoveryStarted();
	virtual void OnRecoveryStarted_Implementation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Charged Shot|Animation")
	FName DefaultSectionName = TEXT("Default");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Charged Shot|Animation")
	FName ChargeSectionName = TEXT("Charge");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Charged Shot|Animation")
	FName RecoverySectionName = TEXT("Recovery");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Charged Shot|Charging", meta = (ClampMin = "0.01"))
	float MaximumChargeTime = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Charged Shot|Charging")
	bool bAutoReleaseAtMaximumCharge = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Charged Shot|Interruption")
	EChargeInterruptionResponse MovementInterruptionResponse = EChargeInterruptionResponse::Cancel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Charged Shot|Interruption", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MovementInterruptionDeadZone = 0.1f;

private:
	bool HandleMaximumCharge();
	bool ConfigureMontageSections();
	bool ValidateMontageSections() const;

	void HandleChargedShotSectionChanged(UAnimMontage* Montage, FName SectionName, bool bLooped);

	UPROPERTY(Transient)
	EChargedShotStage ChargedShotStage = EChargedShotStage::Inactive;

	UPROPERTY(Transient)
	float CurrentChargeTime = 0.0f;

	UPROPERTY(Transient)
	bool bChargeStarted = false;

	UPROPERTY(Transient)
	bool bMaximumChargeReached = false;
};