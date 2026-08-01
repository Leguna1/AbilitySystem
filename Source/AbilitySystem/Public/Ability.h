#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "Ability.generated.h"

class ACharacter;
class UAbilityComponent;
class UMotionWarpingComponent;
class UTargetingComponent;
class UTexture2D;

UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UAbility : public UObject
{
	GENERATED_BODY()

public:
	void InitializeAbility(UAbilityComponent* InAbilityComponent, ACharacter* InOwningCharacter);

	virtual UWorld* GetWorld() const override;

	/* -------------------- Activation -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Lifecycle")
	bool CanActivateAbility() const;
	virtual bool CanActivateAbility_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Lifecycle")
	void ActivateAbility();
	virtual void ActivateAbility_Implementation();

	/* -------------------- Commitment -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Commit")
	bool CanCommitAbility() const;
	virtual bool CanCommitAbility_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Commit")
	void OnAbilityCommitted();
	virtual void OnAbilityCommitted_Implementation();

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

	/* -------------------- Repeated activation -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Requests")
	bool CanHandleRepeatedActivationRequest() const;
	virtual bool CanHandleRepeatedActivationRequest_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Requests")
	bool HandleRepeatedActivationRequest();
	virtual bool HandleRepeatedActivationRequest_Implementation();

	/* -------------------- Transition -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Transition")
	bool CanTransitionTo(const UAbility* IncomingAbility) const;
	virtual bool CanTransitionTo_Implementation(const UAbility* IncomingAbility) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Transition")
	bool IsTransitionOpen() const { return bTransitionOpen; }

	UFUNCTION(BlueprintPure, Category = "Ability|Transition")
	const FGameplayTagContainer& GetAllowedTransitionAbilityTags() const { return AllowedTransitionAbilityTags; }

	/* -------------------- Early cancellation -------------------- */

	/**
	 * Returns whether IncomingAbility may replace this ability before its
	 * early-cancellation window closes.
	 *
	 * The default implementation requires:
	 * - early cancellation not to be closed;
	 * - this ability not to be committed;
	 * - IncomingAbility to match AllowedEarlyCancellationAbilityTags.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Early Cancellation")
	bool CanEarlyCancelTo(const UAbility* IncomingAbility) const;
	virtual bool CanEarlyCancelTo_Implementation(const UAbility* IncomingAbility) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Early Cancellation")
	bool IsEarlyCancellationClosed() const { return bEarlyCancellationClosed; }

	UFUNCTION(BlueprintPure, Category = "Ability|Early Cancellation")
	const FGameplayTagContainer& GetAllowedEarlyCancellationAbilityTags() const { return AllowedEarlyCancellationAbilityTags; }

	/* -------------------- Cancellation -------------------- */

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Cancellation")
	bool CanBeCancelledBy(const UAbility* IncomingAbility) const;
	virtual bool CanBeCancelledBy_Implementation(const UAbility* IncomingAbility) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Cancellation")
	bool CanReplaceActiveAbility(const UAbility* CurrentAbility) const;
	virtual bool CanReplaceActiveAbility_Implementation(const UAbility* CurrentAbility) const;

	/* -------------------- Tick -------------------- */

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Tick")
	void TickAbility(float DeltaTime);
	virtual void TickAbility_Implementation(float DeltaTime);

	/* -------------------- Ending -------------------- */

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Lifecycle")
	void OnAbilityEnded(EAbilityEndReason EndReason);
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason);

	/* -------------------- Getters -------------------- */

	/* -------------------- Display -------------------- */

	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	FText GetDescription() const { return Description; }

	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	UTexture2D* GetIcon() const { return Icon; }

	/** Short glyph shown on the hotbar slot, e.g. "1", "LMB", "RT". Purely cosmetic. */
	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	FText GetKeybindLabel() const { return KeybindLabel; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	FGameplayTag GetAbilityId() const { return AbilityId; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	EAbilityStatus GetAbilityStatus() const { return AbilityStatus; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	bool IsCommitted() const { return bCommitted; }
	
	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	UAbilityComponent* GetAbilityComponent() const { return AbilityComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	ACharacter* GetOwningCharacter() const { return OwningCharacter; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	FGameplayTag GetActivationInputTag() const { return ActivationInputTag; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	float GetInputBufferDuration() const { return InputBufferDuration; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool CanActivateFromHeldInput() const { return bCanActivateFromHeldInput; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool RequiresInputHeldAtResolution() const { return bRequireInputHeldAtResolution; }

	UFUNCTION(BlueprintPure, Category = "Ability|Requests")
	int32 GetActivationPriority() const { return ActivationPriority; }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	const FGameplayTagContainer& GetAbilityTags() const { return AbilityTags; }

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

protected:
	/* -------------------- Ability requests -------------------- */

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

	/* -------------------- Transition control -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Ability|Transition")
	void OpenTransition();

	UFUNCTION(BlueprintCallable, Category = "Ability|Transition")
	void CloseTransition();

	/* -------------------- Early cancellation control -------------------- */

	/**
	 * Permanently closes early cancellation for the current execution.
	 *
	 * The state is reset when a new execution instance activates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Early Cancellation")
	void CloseEarlyCancellation();

	/* -------------------- Tick and queries -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Ability|Tick")
	void SetAbilityTickEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool IsInputHeld(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	FVector2D GetMovementInput() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool OwnerHasTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool OwnerHasAllTags(const FGameplayTagContainer& Tags) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool OwnerHasAnyTags(const FGameplayTagContainer& Tags) const;

	/* -------------------- Definition -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display")
	FText KeybindLabel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FGameplayTag AbilityId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Input")
	FGameplayTag ActivationInputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Input", meta = (ClampMin = "0.0"))
	float InputBufferDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Input")
	bool bCanActivateFromHeldInput = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Input")
	bool bRequireInputHeldAtResolution = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Requests")
	int32 ActivationPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Tags")
	FGameplayTagContainer AbilityTags;

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

	/**
	 * Incoming ability categories that may replace this ability before early
	 * cancellation closes.
	 *
	 * The window starts available, but an empty container allows nothing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Early Cancellation")
	FGameplayTagContainer AllowedEarlyCancellationAbilityTags;
	
	

private:
	friend class UAbilityComponent;

	void SetAbilityStatus(EAbilityStatus NewStatus) { AbilityStatus = NewStatus; }
	void SetCommitted(bool bNewCommitted) { bCommitted = bNewCommitted; }
	void SetTransitionOpen(bool bNewTransitionOpen) { bTransitionOpen = bNewTransitionOpen; }
	void SetEarlyCancellationClosed(bool bNewClosed) { bEarlyCancellationClosed = bNewClosed; }

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

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