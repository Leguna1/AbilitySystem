#pragma once

#include "CoreMinimal.h"
#include "RangedAttackAbility.h"
#include "AbilityProgressProvider.h"
#include "RapidFireAbility.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class ERapidFireStage : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Windup UMETA(DisplayName = "Windup"),
	Firing UMETA(DisplayName = "Firing"),
	Recovery UMETA(DisplayName = "Recovery")
};

UENUM(BlueprintType)
enum class ERapidFireMovementResponse : uint8
{
	Ignore UMETA(DisplayName = "Ignore"),
	Cancel UMETA(DisplayName = "Cancel"),
	Finish UMETA(DisplayName = "Finish")
};

/**
 * Repeated projectile attack using one montage.
 *
 * Required montage sections:
 *
 * Default
 * - plays once;
 * - enters the rapid-fire stance;
 * - naturally continues into FireLoop.
 *
 * FireLoop
 * - prepares, nocks, and releases one projectile per iteration;
 * - repeats until MaximumShots has been fired;
 * - then continues into Recovery.
 *
 * Recovery
 * - contains the post-attack recovery;
 * - may open the Transition window;
 * - ends the ability naturally.
 *
 * Stat.FireRateRamp (base 0): each shot fired speeds the loop up by this much
 * (0.06 = +6% per shot), up to Max Fire Rate Multiplier. Used by Overdrive.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API URapidFireAbility : public URangedAttackAbility, public IAbilityProgressProvider
{
	GENERATED_BODY()

public:
	URapidFireAbility();

	virtual FAbilityProgress GetAbilityProgress_Implementation() const override;

	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnMovementInputReceived_Implementation(FVector2D MovementInput) override;
	virtual void OnAbilityMontageEnded_Implementation(UAnimMontage* Montage, bool bInterrupted) override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;
	virtual bool CanReplaceActiveAbility_Implementation(const UActiveAbility* CurrentAbility) const override;
	virtual void OnCostPaymentFailed_Implementation() override;

	UFUNCTION(BlueprintPure, Category = "Ability|Rapid Fire")
	ERapidFireStage GetRapidFireStage() const { return RapidFireStage; }

	UFUNCTION(BlueprintPure, Category = "Ability|Rapid Fire")
	int32 GetShotsFired() const { return ShotsFired; }

	UFUNCTION(BlueprintPure, Category = "Ability|Rapid Fire")
	int32 GetMaximumShots() const { return MaximumShots; }

	UFUNCTION(BlueprintPure, Category = "Ability|Rapid Fire")
	int32 GetRemainingShots() const { return FMath::Max(MaximumShots - ShotsFired, 0); }

	UFUNCTION(BlueprintPure, Category = "Ability|Rapid Fire")
	float GetRapidFireProgress() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Rapid Fire")
	bool IsFinishRequested() const { return bFinishRequested; }

	/** The arrow about to be released is the last of a full volley (not one cut short by moving). */
	UFUNCTION(BlueprintPure, Category = "Ability|Rapid Fire")
	bool IsFinalShotPending() const;

	/** Current loop speed from the fire rate ramp: 1 = unramped. */
	UFUNCTION(BlueprintPure, Category = "Ability|Rapid Fire")
	float GetFireRateMultiplier() const { return CurrentFireRateMultiplier; }

protected:
	virtual void OnProjectileReleased_Implementation(float Strength) override;

	/**
	 * Stops repeating FireLoop after its current iteration and continues into
	 * Recovery.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Rapid Fire")
	bool RequestFinishRapidFire();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Rapid Fire")
	void OnRapidFireStarted();
	virtual void OnRapidFireStarted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Rapid Fire")
	void OnFiringLoopStarted();
	virtual void OnFiringLoopStarted_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Rapid Fire")
	void OnRapidFireShotReleased(int32 ShotNumber, int32 MaxShots, float Strength);
	virtual void OnRapidFireShotReleased_Implementation(int32 ShotNumber, int32 MaxShots, float Strength);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Rapid Fire")
	void OnRapidFireFinishRequested();
	virtual void OnRapidFireFinishRequested_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Rapid Fire")
	void OnRapidFireRecoveryStarted();
	virtual void OnRapidFireRecoveryStarted_Implementation();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rapid Fire|Animation")
	FName DefaultSectionName = TEXT("Default");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rapid Fire|Animation")
	FName FireLoopSectionName = TEXT("FireLoop");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rapid Fire|Animation")
	FName RecoverySectionName = TEXT("Recovery");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rapid Fire")
	FAbilityRankedInt MaximumShotsByRank = FAbilityRankedInt(6);

	/**
	 * Determines what movement input does while Rapid Fire is active.
	 *
	 * Ignore:
	 * - movement does not affect the ability.
	 *
	 * Cancel:
	 * - the ability ends immediately as cancelled.
	 *
	 * Finish:
	 * - Default movement cancels;
	 * - FireLoop movement allows the current shot cycle to finish, then enters
	 *   Recovery.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rapid Fire|Interruption")
	ERapidFireMovementResponse MovementResponse = ERapidFireMovementResponse::Finish;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rapid Fire|Interruption", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MovementInterruptionDeadZone = 0.1f;

	/** The fire rate ramp (Stat.FireRateRamp) never speeds the loop past this multiple of its starting speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rapid Fire|Fire Rate", meta = (ClampMin = "1.0"))
	float MaxFireRateMultiplier = 2.0f;

private:
	bool ConfigureMontageSections();
	bool ValidateMontageSections() const;

	void HandleRapidFireSectionChanged(UAnimMontage* Montage, FName SectionName, bool bLooped);

	/** Applies the fire rate ramp for the shots fired so far. */
	void ApplyFireRateRamp();

	UPROPERTY(Transient)
	ERapidFireStage RapidFireStage = ERapidFireStage::Inactive;

	UPROPERTY(Transient)
	int32 ShotsFired = 0;

	UPROPERTY(Transient)
	bool bFinishRequested = false;

	UPROPERTY(Transient)
	bool bFiringLoopStarted = false;
	
	/** Resolved from MaximumShotsByRank at activation. */
	UPROPERTY(Transient)
	int32 MaximumShots = 0;

	/** Stat.FireRateRamp, resolved at activation. */
	UPROPERTY(Transient)
	float ResolvedFireRateRamp = 0.0f;

	/** Montage play rate when the loop started (attack speed included); the ramp scales this. */
	UPROPERTY(Transient)
	float LoopBasePlayRate = 1.0f;

	UPROPERTY(Transient)
	float CurrentFireRateMultiplier = 1.0f;
};