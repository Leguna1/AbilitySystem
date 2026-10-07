#pragma once

#include "CoreMinimal.h"
#include "Ability.h"
#include "AbilityFragment.h"
#include "ActiveAbility.generated.h"

class UAnimSequenceBase;
class UMotionWarpingComponent;
class UTargetingComponent;
class UAbilityFragment;

/**
 * An ability the character activates: input, cost and cooldown, commit,
 * transitions, early cancellation and replacement rules. A fresh instance
 * is created for every execution.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UActiveAbility : public UAbility
{
	GENERATED_BODY()

public:
	virtual void InitializeAbility(UAbilityComponent* InAbilityComponent, ACharacter* InOwningCharacter) override;

	/* -------------------- Activation -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Lifecycle")
	bool CanActivateAbility() const;
	virtual bool CanActivateAbility_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Lifecycle")
	void ActivateAbility();
	virtual void ActivateAbility_Implementation();

	/* -------------------- Commitment and cost -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Commit")
	bool CanCommitAbility() const;
	virtual bool CanCommitAbility_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Commit")
	void OnAbilityCommitted();
	virtual void OnAbilityCommitted_Implementation();

	/**
	 * A cost payment failed (the owner could not afford it). The event that
	 * triggered it was not delivered. Default: end the ability as Failed.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Cost")
	void OnCostPaymentFailed();
	virtual void OnCostPaymentFailed_Implementation();

	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	EAbilityCostTrigger GetCostTrigger() const { return CostTrigger; }

	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	EAbilityCooldownTrigger GetCooldownTrigger() const { return CooldownTrigger; }

	/** Event that pays the cost for the animation-event triggers. Subclasses supply a fallback when unset. */
	virtual FGameplayTag GetCostEventTag() const { return CostEventTag; }

	/* -------------------- Input -------------------- */

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Input")
	void OnInputPressed(FGameplayTag InputTag);
	virtual void OnInputPressed_Implementation(FGameplayTag InputTag);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Input")
	void OnInputReleased(FGameplayTag InputTag);
	virtual void OnInputReleased_Implementation(FGameplayTag InputTag);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Input")
	void OnMovementInputReceived(FVector2D MovementInput);
	virtual void OnMovementInputReceived_Implementation(FVector2D MovementInput);

	/* -------------------- Animation events -------------------- */

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Animation")
	void OnAnimationEvent(FGameplayTag EventTag);
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag);

	/** Whether an event from SourceAnimation belongs to this execution. Null source = manual event, always accepted. */
	virtual bool AcceptsAnimationEvent(const UAnimSequenceBase* SourceAnimation) const { return true; }

	/* -------------------- Repeated activation -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Requests")
	bool CanHandleRepeatedActivationRequest() const;
	virtual bool CanHandleRepeatedActivationRequest_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Requests")
	bool HandleRepeatedActivationRequest();
	virtual bool HandleRepeatedActivationRequest_Implementation();

	/* -------------------- Transition -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Transition")
	bool CanTransitionTo(const UActiveAbility* IncomingAbility) const;
	virtual bool CanTransitionTo_Implementation(const UActiveAbility* IncomingAbility) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Transition")
	bool IsTransitionOpen() const { return bTransitionOpen; }

	UFUNCTION(BlueprintPure, Category = "Ability|Transition")
	const FGameplayTagContainer& GetAllowedTransitionAbilityTags() const { return AllowedTransitionAbilityTags; }

	/* -------------------- Early cancellation -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Early Cancellation")
	bool CanEarlyCancelTo(const UActiveAbility* IncomingAbility) const;
	virtual bool CanEarlyCancelTo_Implementation(const UActiveAbility* IncomingAbility) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Early Cancellation")
	bool IsEarlyCancellationClosed() const { return bEarlyCancellationClosed; }

	UFUNCTION(BlueprintPure, Category = "Ability|Early Cancellation")
	const FGameplayTagContainer& GetAllowedEarlyCancellationAbilityTags() const { return AllowedEarlyCancellationAbilityTags; }

	/* -------------------- Cancellation -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Cancellation")
	bool CanBeCancelledBy(const UActiveAbility* IncomingAbility) const;
	virtual bool CanBeCancelledBy_Implementation(const UActiveAbility* IncomingAbility) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Cancellation")
	bool CanReplaceActiveAbility(const UActiveAbility* CurrentAbility) const;
	virtual bool CanReplaceActiveAbility_Implementation(const UActiveAbility* CurrentAbility) const;

	/* -------------------- Tick and ending -------------------- */

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Tick")
	void TickAbility(float DeltaTime);
	virtual void TickAbility_Implementation(float DeltaTime);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Lifecycle")
	void OnAbilityEnded(EAbilityEndReason EndReason);
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason);

	/* -------------------- Getters -------------------- */

	/** Short glyph shown on the hotbar slot, e.g. "1", "LMB", "RT". Purely cosmetic. */
	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	FText GetKeybindLabel() const { return KeybindLabel; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	EAbilityStatus GetAbilityStatus() const { return AbilityStatus; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	bool IsCommitted() const { return bCommitted; }

	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	FGameplayTag GetActivationInputTag() const { return ActivationInputTag; }

	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	float GetFocusCost() const { return FocusCost; }

	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	float GetCooldownDuration() const { return CooldownDuration; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	float GetInputBufferDuration() const { return InputBufferDuration; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool CanActivateFromHeldInput() const { return bCanActivateFromHeldInput; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool RequiresInputHeldAtResolution() const { return bRequireInputHeldAtResolution; }

	UFUNCTION(BlueprintPure, Category = "Ability|Requests")
	int32 GetActivationPriority() const { return ActivationPriority; }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	const FGameplayTagContainer& GetRequiredOwnerTags() const { return RequiredOwnerTags; }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	const FGameplayTagContainer& GetBlockedOwnerTags() const { return BlockedOwnerTags; }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	const FGameplayTagContainer& GetGrantedOwnerTags() const { return GrantedOwnerTags; }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	const FGameplayTagContainer& GetBlockAbilitiesWithTags() const { return BlockAbilitiesWithTags; }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	const FGameplayTagContainer& GetCancelAbilitiesWithTags() const { return CancelAbilitiesWithTags; }
	
	/* -------------------- Features -------------------- */

	const TArray<TObjectPtr<UAbilityFragment>>& GetFragments() const { return Fragments; }

	/** First fragment of type T, or null. */
	template <typename T>
	const T* FindFragment() const
	{
		for (const UAbilityFragment* Fragment : Fragments)
		{
			if (const T* Typed = Cast<T>(Fragment))
			{
				return Typed;
			}
		}

		return nullptr;
	}

	/** Calls Visitor for every fragment of type T. */
	template <typename T>
	void ForEachFragment(TFunctionRef<void(const T&)> Visitor) const
	{
		for (const UAbilityFragment* Fragment : Fragments)
		{
			if (const T* Typed = Cast<T>(Fragment))
			{
				Visitor(*Typed);
			}
		}
	}

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/* -------------------- Requests -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Ability|Requests")
	bool RequestCommit();

	UFUNCTION(BlueprintCallable, Category = "Ability|Requests")
	void RequestEndAbility();

	UFUNCTION(BlueprintCallable, Category = "Ability|Requests")
	void RequestCancelAbility();

	UFUNCTION(BlueprintCallable, Category = "Ability|Requests")
	bool RequestAbility(FGameplayTag RequestedAbilityId);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	bool RequestResolveBufferedInput();

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void ClearBufferedInputs();

	UFUNCTION(BlueprintCallable, Category = "Ability|Transition")
	void OpenTransition();

	UFUNCTION(BlueprintCallable, Category = "Ability|Transition")
	void CloseTransition();

	/** Permanently closes early cancellation for this execution. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Early Cancellation")
	void CloseEarlyCancellation();

	UFUNCTION(BlueprintCallable, Category = "Ability|Tick")
	void SetAbilityTickEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool IsInputHeld(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	FVector2D GetMovementInput() const;

	/* -------------------- Definition -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display")
	FText KeybindLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Input")
	FGameplayTag ActivationInputTag;

	/** Focus spent per payment (see CostTrigger). Affordability is checked at activation and before every payment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Cost", meta = (ClampMin = "0.0"))
	float FocusCost = 0.0f;

	/** Seconds this ability is unavailable after its cooldown starts. 0 = no cooldown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Cost", meta = (ClampMin = "0.0"))
	float CooldownDuration = 0.0f;

	/** When FocusCost is paid. The first payment commits the ability (point of no return). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Cost")
	EAbilityCostTrigger CostTrigger = EAbilityCostTrigger::Manual;

	/** Event that pays the cost. Empty = the ability's default (ranged: release, melee: hit window start). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Cost", meta = (EditCondition = "CostTrigger == EAbilityCostTrigger::OnAnimationEvent || CostTrigger == EAbilityCostTrigger::OnEveryAnimationEvent"))
	FGameplayTag CostEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Cost")
	EAbilityCooldownTrigger CooldownTrigger = EAbilityCooldownTrigger::OnCommit;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Input", meta = (ClampMin = "0.0"))
	float InputBufferDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Input")
	bool bCanActivateFromHeldInput = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Input")
	bool bRequireInputHeldAtResolution = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Requests")
	int32 ActivationPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Tags")
	FGameplayTagContainer RequiredOwnerTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Tags")
	FGameplayTagContainer BlockedOwnerTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Tags")
	FGameplayTagContainer GrantedOwnerTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Tags")
	FGameplayTagContainer BlockAbilitiesWithTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Tags")
	FGameplayTagContainer CancelAbilitiesWithTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Transition")
	FGameplayTagContainer AllowedTransitionAbilityTags;

	/** Incoming abilities that may replace this one before early cancellation closes. Empty allows nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Early Cancellation")
	FGameplayTagContainer AllowedEarlyCancellationAbilityTags;
	
	/** Tells the owner's passives that something happened (e.g. Event.Combo.Finished). */
	UFUNCTION(BlueprintCallable, Category = "Ability|Events")
	void SendGameplayEvent(FGameplayTag EventTag);
	
	/** Optional features for this ability. Click + and pick one to add it, e.g. Pierce or Apply Status on Hit. */
	UPROPERTY(EditDefaultsOnly, Instanced, BlueprintReadOnly, Category = "Features")
	TArray<TObjectPtr<UAbilityFragment>> Fragments;

private:
	friend class UAbilityComponent;

	void SetAbilityStatus(EAbilityStatus NewStatus) { AbilityStatus = NewStatus; }
	void SetCommitted(bool bNewCommitted) { bCommitted = bNewCommitted; }
	void SetTransitionOpen(bool bNewTransitionOpen) { bTransitionOpen = bNewTransitionOpen; }
	void SetEarlyCancellationClosed(bool bNewClosed) { bEarlyCancellationClosed = bNewClosed; }
	
	void NotifyFragmentsStarted();
	void NotifyFragmentsEnded(EAbilityEndReason EndReason);

	UPROPERTY(Transient)
	EAbilityStatus AbilityStatus = EAbilityStatus::Inactive;

	UPROPERTY(Transient)
	bool bCommitted = false;

	UPROPERTY(Transient)
	bool bTransitionOpen = false;

	UPROPERTY(Transient)
	bool bEarlyCancellationClosed = false;

	UPROPERTY(Transient)
	TObjectPtr<UTargetingComponent> TargetingComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;
	
};