#pragma once

#include "CoreMinimal.h"
#include "Ability.h"
#include "AbilityTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InputBufferTypes.h"
#include "AbilityComponent.generated.h"

class ACharacter;
class UAbility;
class UInputBufferComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAbilityActivatedEventSignature, FGameplayTag, AbilityId, UAbility*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAbilityCommittedEventSignature, FGameplayTag, AbilityId, UAbility*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAbilityEndedEventSignature, FGameplayTag, AbilityId, UAbility*, Ability, EAbilityEndReason, EndReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOwnedTagsChangedEventSignature, FGameplayTagContainer, OwnedTags);

UCLASS(ClassGroup = (Ability), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UAbility;

public:
	UAbilityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Ability|Granted")
	bool GrantAbility(TSubclassOf<UAbility> AbilityClass);

	UFUNCTION(BlueprintCallable, Category = "Ability|Granted")
	bool RemoveAbility(FGameplayTag AbilityId);

	UFUNCTION(BlueprintPure, Category = "Ability|Granted")
	bool HasAbility(FGameplayTag AbilityId) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Granted")
	TSubclassOf<UAbility> FindAbilityClassById(FGameplayTag AbilityId) const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	bool TryActivateAbility(FGameplayTag AbilityId);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	bool ResolveBufferedAbilityInput();

	UFUNCTION(BlueprintCallable, Category = "Ability")
	void CancelActiveAbility();

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void InputPressed(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void InputReleased(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void MovementInputReceived(FVector2D MovementInput);

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool IsInputHeld(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void ClearBufferedInputs();

	UFUNCTION(BlueprintCallable, Category = "Ability|Events")
	void HandleAbilityEvent(FGameplayTag EventTag);

	UFUNCTION(BlueprintCallable, Category = "Ability|Tags")
	void AddLooseOwnerTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Ability|Tags")
	void RemoveLooseOwnerTag(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool HasOwnerTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool HasAllOwnerTags(const FGameplayTagContainer& Tags) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool HasAnyOwnerTags(const FGameplayTagContainer& Tags) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	FGameplayTagContainer GetOwnedGameplayTags() const;

	UFUNCTION(BlueprintPure, Category = "Ability")
	UAbility* GetActiveAbility() const { return ActiveAbility; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	bool HasActiveAbility() const { return IsValid(ActiveAbility); }

	UFUNCTION(BlueprintPure, Category = "Ability")
	bool IsActiveAbilityCommitted() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	UInputBufferComponent* GetInputBufferComponent() const { return InputBufferComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	FVector2D GetMovementInput() const;

	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FAbilityActivatedEventSignature AbilityActivatedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FAbilityCommittedEventSignature AbilityCommittedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FAbilityEndedEventSignature AbilityEndedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FOwnedTagsChangedEventSignature OwnedTagsChangedEvent;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	struct FAbilityInputCandidate
	{
		TSubclassOf<UAbility> AbilityClass;
		FBufferedInput BufferedInput;
		int32 Priority = 0;
	};

	bool CommitAbility(UAbility* RequestingAbility);
	void EndAbility(UAbility* RequestingAbility, EAbilityEndReason EndReason);
	void SetAbilityTickEnabled(UAbility* RequestingAbility, bool bEnabled);
	void SetAbilityTransitionOpen(UAbility* RequestingAbility, bool bOpen);
	void SetAbilityEarlyCancellationClosed(UAbility* RequestingAbility, bool bClosed);

	void BuildAbilityInputCandidates(TArray<FAbilityInputCandidate>& OutCandidates);
	bool ExecuteAbilityInputCandidate(const FAbilityInputCandidate& Candidate);
	bool ExecuteRepeatedAbilityRequest(UAbility* Ability);
	bool ReplaceActiveAbility(TSubclassOf<UAbility> IncomingAbilityClass);

	bool CanActivateAbilityInstance(const UAbility* Ability, bool bReplacingActiveAbility) const;
	bool CanReplaceActiveAbility(const UAbility* CurrentAbility, const UAbility* IncomingAbility, EAbilityEndReason& OutReplacementReason) const;
	UAbility* CreateExecutionInstance(TSubclassOf<UAbility> AbilityClass);
	bool ActivateAbilityInstance(UAbility* Ability);
	void EndActiveAbilityInternal(EAbilityEndReason EndReason);

	void DispatchAbilityCallback(const TFunctionRef<void()>& Callback);

	FGameplayTagContainer BuildLooseOwnerTags() const;
	FGameplayTagContainer BuildOwnedTagsWithoutActiveAbility() const;

	void ApplyActiveAbilityTags();
	void RemoveActiveAbilityTags();
	void BroadcastOwnedTagsChanged();

	const UAbility* GetAbilityCDO(TSubclassOf<UAbility> AbilityClass) const;
	float GetLongestBufferDurationForInput(FGameplayTag InputTag) const;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Granted")
	TArray<TSubclassOf<UAbility>> StartingAbilityClasses;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferComponent> InputBufferComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|Granted", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UAbility>> GrantedAbilityClasses;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbility> ActiveAbility;

	UPROPERTY(Transient)
	TMap<FGameplayTag, int32> LooseOwnerTagCounts;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|Tags", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ActiveGrantedTags;

	bool bAbilityTickEnabled = false;
	bool bDispatchingAbilityCallback = false;
	bool bResolvingBufferedInput = false;
	bool bEndingAbility = false;
	bool bResolveBufferedInputAfterCallback = false;
};