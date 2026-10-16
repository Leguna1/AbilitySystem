#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ResourceComponent.generated.h"

class UAttributeComponent;

UENUM(BlueprintType)
enum class EResourceType : uint8
{
	Health UMETA(DisplayName = "Health"),
	Focus  UMETA(DisplayName = "Focus")
};

UENUM(BlueprintType)
enum class EResourceValueType : uint8
{
	Current UMETA(DisplayName = "Current"),
	Max     UMETA(DisplayName = "Max")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOnResourceChanged,
	EResourceType, ResourceType,
	EResourceValueType, ValueType,
	float, OldValue,
	float, NewValue
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRevived);

UCLASS(ClassGroup = (Ability), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UResourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UResourceComponent();

	/**
	 * Returns either the Current or Max value of the requested resource.
	 */
	UFUNCTION(BlueprintPure, Category = "Resources")
	float GetResourceValue(
		EResourceType ResourceType,
		EResourceValueType ValueType
	) const;

	/**
	 * Adds ModifyValue to either the Current or Max resource value.
	 *
	 * Examples:
	 * ModifyResource(Health, Current, -20) = take 20 damage.
	 * ModifyResource(Health, Current, 10)  = heal 10 health.
	 * ModifyResource(Focus, Max, 25)       = increase max focus by 25.
	 */
	UFUNCTION(BlueprintCallable, Category = "Resources")
	float ModifyResource(
		EResourceType ResourceType,
		EResourceValueType ValueType,
		float ModifyValue
	);

	/**
	 * Stops all active regeneration timers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Resources|Regeneration")
	void StopAllRegeneration();

	/**
	 * Restarts the normal Focus regeneration timer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Resources|Regeneration")
	void RestartFocusRegeneration();

	UPROPERTY(BlueprintAssignable, Category = "Resources|Events")
	FOnResourceChanged OnResourceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Resources|Events")
	FOnDeath OnDeath;
	
	UFUNCTION(BlueprintPure, Category = "Resources")
	bool IsDead() const { return bIsDead; }

	/** Brings a dead owner back with HealthFraction of max health. Healing alone never revives. */
	UFUNCTION(BlueprintCallable, Category = "Resources")
	bool Revive(float HealthFraction = 1.0f);

	UPROPERTY(BlueprintAssignable, Category = "Resources|Events")
	FOnRevived OnRevived;

	/** Fills Health and Focus to max (not while dead: that's Revive). Used on level up. */
	UFUNCTION(BlueprintCallable, Category = "Resources")
	void RefillResources();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason
	) override;

	/* -------------------- Max from attributes -------------------- */

	/**
	 * When the Attribute Component has this attribute, it decides Max Health (levels, Vitality, buffs).
	 * Max Health below is then only the fallback for characters without it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Attributes", meta = (Categories = "Stat"))
	FGameplayTag MaxHealthAttribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resources|Attributes", meta = (Categories = "Stat"))
	FGameplayTag MaxFocusAttribute;

	/* -------------------- Health -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Resources|Health",
		meta = (ClampMin = "0.0")
	)
	float MaxHealth = 100.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Resources|Health",
		meta = (ClampMin = "0.0")
	)
	float CurrentHealth = 100.0f;

	/**
	 * Whether Health automatically regenerates.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Resources|Health Regeneration"
	)
	bool bEnableHealthRegeneration = true;

	/**
	 * Time after Health is reduced before regeneration begins.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Resources|Health Regeneration",
		meta = (
			ClampMin = "0.0",
			EditCondition = "bEnableHealthRegeneration"
		)
	)
	float HealthRegenerationDelay = 6.0f;

	/**
	 * Time between each Health regeneration application.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Resources|Health Regeneration",
		meta = (
			ClampMin = "0.01",
			EditCondition = "bEnableHealthRegeneration"
		)
	)
	float HealthRegenerationInterval = 1.0f;

	/**
	 * Percentage of Max Health restored each interval.
	 *
	 * Example:
	 * 2.0 means 2% of Max Health.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Resources|Health Regeneration",
		meta = (
			ClampMin = "0.0",
			EditCondition = "bEnableHealthRegeneration"
		)
	)
	float HealthRegenerationPercentOfMax = 2.0f;

	/* -------------------- Focus -------------------- */

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Resources|Focus",
		meta = (ClampMin = "0.0")
	)
	float MaxFocus = 100.0f;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Resources|Focus",
		meta = (ClampMin = "0.0")
	)
	float CurrentFocus = 100.0f;

	/**
	 * Whether Focus automatically regenerates.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Resources|Focus Regeneration"
	)
	bool bEnableFocusRegeneration = true;

	/**
	 * Time between each Focus regeneration application.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Resources|Focus Regeneration",
		meta = (
			ClampMin = "0.01",
			EditCondition = "bEnableFocusRegeneration"
		)
	)
	float FocusRegenerationInterval = 1.0f;

	/**
	 * Fixed amount of Focus restored each interval.
	 */
	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Resources|Focus Regeneration",
		meta = (
			ClampMin = "0.0",
			EditCondition = "bEnableFocusRegeneration"
		)
	)
	float FocusRegenerationAmount = 5.0f;

private:
	FTimerHandle HealthRegenerationDelayTimer;
	FTimerHandle HealthRegenerationTimer;
	FTimerHandle FocusRegenerationTimer;

	bool bIsDead = false;

	float ModifyCurrentResource(
		EResourceType ResourceType,
		float ModifyValue
	);

	float ModifyMaxResource(
		EResourceType ResourceType,
		float ModifyValue
	);

	void HandleHealthReduced();

	UFUNCTION()
	void HandleAttributeChanged(FGameplayTag Attribute, float OldValue, float NewValue);

	/** Sets Max to NewMax. A higher max also adds the difference to Current, so growing never looks like damage. */
	void ApplyMaxFromAttribute(EResourceType ResourceType, float NewMax);

	/** At start: Max from the attribute, Current kept at the same fraction (full stays full). */
	void InitializeMaxFromAttribute(EResourceType ResourceType, FGameplayTag Attribute);

	UPROPERTY(Transient)
	TObjectPtr<UAttributeComponent> AttributeComponent;

	void StartHealthRegeneration();
	void RegenerateHealth();
	void RegenerateFocus();

	void StopHealthRegeneration();
	void StopFocusRegeneration();

	void BroadcastResourceChange(
		EResourceType ResourceType,
		EResourceValueType ValueType,
		float OldValue,
		float NewValue
	);

	void EvaluateDeath(
		float OldHealth,
		float NewHealth
	);
};