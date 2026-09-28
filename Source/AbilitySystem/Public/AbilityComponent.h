#pragma once

#include "CoreMinimal.h"
#include "ActiveAbility.h"
#include "AbilityTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InputBufferTypes.h"
#include "UObject/ObjectKey.h"
#include "ModifierTypes.h"
#include "AbilityComponent.generated.h"

class ACharacter;
class UAbility;
class UInputBufferComponent;
class UMotionWarpingComponent;
class UResourceComponent;
class UTargetingComponent;
class UAnimSequenceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAbilityActivatedEventSignature, FGameplayTag, AbilityId, UActiveAbility*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAbilityCommittedEventSignature, FGameplayTag, AbilityId, UActiveAbility*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAbilityEndedEventSignature, FGameplayTag, AbilityId, UActiveAbility*, Ability, EAbilityEndReason, EndReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOwnedTagsChangedEventSignature, FGameplayTagContainer, OwnedTags);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGrantedAbilitiesChangedEventSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAbilityRankChangedEventSignature, FGameplayTag, AbilityId, int32, OldRank, int32, NewRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FModifiersChangedEventSignature);

/** Parallel-array cooldown store keyed by ability id (no TMap by project convention). */
USTRUCT()
struct FAbilityCooldownState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGameplayTag> AbilityIds;

	/** World time (seconds) at which the matching ability leaves cooldown. */
	UPROPERTY()
	TArray<double> EndTimes;
	
	/** Duration each cooldown started with, so the sweep stays smooth if modifiers change mid-cooldown. */
	UPROPERTY()
	TArray<float> Durations;
};
/** Learned ranks keyed by ability id. Effective rank = StartingRank + learned, clamped to MaxRank. */
USTRUCT()
struct FAbilityRankState
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FGameplayTag> AbilityIds;

	UPROPERTY()
	TArray<int32> LearnedRanks;
};

UCLASS(ClassGroup = (Ability), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UActiveAbility;

public:
	UAbilityComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	

	/** Grants AbilityClass on behalf of Source (null = this component). A class stays granted while any source holds it. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Granted")
	bool GrantAbility(TSubclassOf<UAbility> AbilityClass, const UObject* Source = nullptr);

	/** Batch grant; broadcasts GrantedAbilitiesChangedEvent at most once. Returns how many were granted. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Granted")
	int32 GrantAbilities(const TArray<TSubclassOf<UAbility>>& AbilityClasses, const UObject* Source = nullptr);

	/** Drops Source's hold on the ability. The class is removed when no source holds it. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Granted")
	bool RemoveAbility(FGameplayTag AbilityId, const UObject* Source = nullptr);

	/** Drops every hold Source has. Cancels the active ability if it loses its last source, without resolving buffered input. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Granted")
	int32 RevokeAbilitiesFromSource(const UObject* Source);

	UFUNCTION(BlueprintPure, Category = "Ability|Granted")
	bool IsAbilityGrantedBySource(FGameplayTag AbilityId, const UObject* Source) const;
	

	UFUNCTION(BlueprintPure, Category = "Ability|Granted")
	bool HasAbility(FGameplayTag AbilityId) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Granted")
	TSubclassOf<UAbility> FindAbilityClassById(FGameplayTag AbilityId) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Granted")
	const TArray<TSubclassOf<UAbility>>& GetGrantedAbilityClasses() const { return GrantedAbilityClasses; }

	/** Returns the class default object for an ability class, for reading display/definition data without instancing. */
	UFUNCTION(BlueprintPure, Category = "Ability|Granted")
	const UAbility* GetAbilityDefaults(TSubclassOf<UAbility> AbilityClass) const;

	UFUNCTION(BlueprintCallable, Category = "Ability")
	bool TryActivateAbility(FGameplayTag AbilityId);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	bool ResolveBufferedAbilityInput();

	/** Cancels the active ability. Death, stuns and hit reactions pass false so a buffered attack can't fire through them. */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	void CancelActiveAbility(bool bResolveBufferedInput = true);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void MovementInputReceived(FVector2D MovementInput);

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool IsInputHeld(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void ClearBufferedInputs();

	/** SourceAnimation lets the active ability reject events from animations that aren't its own (e.g. a blending-out montage). */
	UFUNCTION(BlueprintCallable, Category = "Ability|Events")
	void HandleAbilityEvent(FGameplayTag EventTag, const UAnimSequenceBase* SourceAnimation = nullptr);

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
	UActiveAbility* GetActiveAbility() const { return ActiveAbility; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	bool HasActiveAbility() const { return IsValid(ActiveAbility); }

	UFUNCTION(BlueprintPure, Category = "Ability")
	bool IsActiveAbilityCommitted() const;

	/* -------------------- Cooldown / Cost -------------------- */

	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	bool IsAbilityOnCooldown(FGameplayTag AbilityId) const;

	/** Seconds remaining on cooldown, 0 if ready. */
	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	float GetAbilityCooldownRemaining(FGameplayTag AbilityId) const;

	/** 0..1 fraction of cooldown elapsed (1 = ready), for a radial sweep. */
	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	float GetAbilityCooldownProgress(FGameplayTag AbilityId) const;

	/** True if the owner currently has enough Focus for this ability's cost. */
	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	bool CanAffordAbility(FGameplayTag AbilityId) const;
	
	/* -------------------- Rank -------------------- */

	/** Effective rank. Works for classes that are not currently granted (e.g. another weapon's kit). */
	UFUNCTION(BlueprintPure, Category = "Ability|Rank")
	int32 GetAbilityRank(TSubclassOf<UAbility> AbilityClass) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Rank")
	int32 GetLearnedRank(TSubclassOf<UAbility> AbilityClass) const;

	/** Sets learned ranks, clamped so the effective rank stays within MaxRank. Returns the new effective rank. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Rank")
	int32 SetLearnedRank(TSubclassOf<UAbility> AbilityClass, int32 LearnedRank);

	UFUNCTION(BlueprintPure, Category = "Ability|Rank")
	bool IsAbilityUnlocked(TSubclassOf<UAbility> AbilityClass) const { return GetAbilityRank(AbilityClass) > 0; }

	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FAbilityRankChangedEventSignature AbilityRankChangedEvent;

	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UResourceComponent* GetResourceComponent() const { return ResourceComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	UInputBufferComponent* GetInputBufferComponent() const { return InputBufferComponent; }
	
	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }
	
	

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
	
	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FGrantedAbilitiesChangedEventSignature GrantedAbilitiesChangedEvent;
	
	/** Single entry point for ability input: true on press, false on release. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void HandleInput(FGameplayTag InputTag, bool bPressed);
	
	/* -------------------- Modifiers -------------------- */

	/**
	 * Adds or replaces Source's entry under EntryKey (null source = this component).
	 * Magnitudes are read at Rank. Duration <= 0 = until removed.
	 * GrantedTags are owner tags for as long as the entry is active.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Modifiers")
	void ApplyModifiers(const UObject* Source, FName EntryKey, const TArray<FStatModifier>& Modifiers, int32 Rank, const FGameplayTagContainer& GrantedTags, float Duration = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Ability|Modifiers")
	bool RemoveModifierEntry(const UObject* Source, FName EntryKey);

	UFUNCTION(BlueprintCallable, Category = "Ability|Modifiers")
	bool RemoveModifiersFromSource(const UObject* Source);

	UFUNCTION(BlueprintPure, Category = "Ability|Modifiers")
	bool HasModifierEntry(const UObject* Source, FName EntryKey) const;

	/** (Base + all Adds) x (1 + all Percents), for modifiers scoped to AbilityTags whose owner-tag conditions hold. */
	UFUNCTION(BlueprintPure, Category = "Ability|Modifiers")
	float GetModifiedValue(FGameplayTag Stat, float BaseValue, const FGameplayTagContainer& AbilityTags) const;

	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FModifiersChangedEventSignature ModifiersChangedEvent;
	
	/** Focus cost per payment, including modifiers. What the hotbar shows and what a payment spends. */
	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	float GetAbilityFocusCost(FGameplayTag AbilityId) const;

protected:
	virtual void BeginPlay() override;
	virtual void InitializeComponent() override;
	
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

private:
	struct FAbilityInputCandidate
	{
		TSubclassOf<UAbility> AbilityClass;
		FBufferedInput BufferedInput;
		int32 Priority = 0;
	};

	bool CommitAbility(UActiveAbility* RequestingAbility);
	void EndAbility(UActiveAbility* RequestingAbility, EAbilityEndReason EndReason);
	void SetAbilityTickEnabled(UActiveAbility* RequestingAbility, bool bEnabled);
	void SetAbilityTransitionOpen(UActiveAbility* RequestingAbility, bool bOpen);
	void SetAbilityEarlyCancellationClosed(UActiveAbility* RequestingAbility, bool bClosed) const;

	void BuildAbilityInputCandidates(TArray<FAbilityInputCandidate>& OutCandidates);
	bool ExecuteAbilityInputCandidate(const FAbilityInputCandidate& Candidate);
	bool ExecuteRepeatedAbilityRequest(UActiveAbility* Ability);
	bool ReplaceActiveAbility(TSubclassOf<UAbility> IncomingAbilityClass);

	bool CanActivateAbilityInstance(const UActiveAbility* Ability, bool bReplacingActiveAbility) const;
	static bool CanReplaceActiveAbility(const UActiveAbility* CurrentAbility, const UActiveAbility* IncomingAbility, EAbilityEndReason& OutReplacementReason);
	/** Null for passive classes: only active abilities have executions. */
	UActiveAbility* CreateExecutionInstance(TSubclassOf<UAbility> AbilityClass);
	bool ActivateAbilityInstance(UActiveAbility* Ability);
	void EndActiveAbilityInternal(EAbilityEndReason EndReason, bool bResolveBufferedInput = true);

	struct FAbilityGrantSources
	{
		TArray<FObjectKey> Sources;
	};

	const UObject* ResolveGrantSource(const UObject* Source) const;
	int32 FindGrantIndex(TSubclassOf<UAbility> AbilityClass) const;
	bool AddGrantInternal(TSubclassOf<UAbility> AbilityClass, const UObject* Source, bool& bOutClassAdded);
	void EraseEmptyGrants(const TArray<TSubclassOf<UAbility>>& Candidates);

	/** Parallel to GrantedAbilityClasses: everything currently granting each entry. */
	TArray<FAbilityGrantSources> GrantedAbilitySources;
	
	void DispatchAbilityCallback(const TFunctionRef<void()>& Callback);
	
	

	FGameplayTagContainer BuildLooseOwnerTags() const;
	FGameplayTagContainer BuildOwnedTagsWithoutActiveAbility() const;

	void ApplyActiveAbilityTags();
	void RemoveActiveAbilityTags();
	void BroadcastOwnedTagsChanged() const;

	static const UAbility* GetAbilityCDO(TSubclassOf<UAbility> AbilityClass);
	float GetLongestBufferDurationForInput(FGameplayTag InputTag) const;

	int32 FindCooldownIndex(FGameplayTag AbilityId) const;
	void StartCooldown(FGameplayTag AbilityId, float Duration);
	void SpendAbilityCost(const UActiveAbility* Ability);
	bool ChargeRepeatedCost(const UActiveAbility* Ability);

	/** Applies the ability's cost trigger for this event. False = payment failed; the event must not be delivered. */
	bool ApplyEventCost(UActiveAbility* Ability, FGameplayTag EventTag);
	
	float GetCurrentFocus() const;

	UPROPERTY(EditDefaultsOnly, Category = "Ability|Granted")
	TArray<TSubclassOf<UAbility>> StartingAbilityClasses;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputBufferComponent> InputBufferComponent;
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|References", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UResourceComponent> ResourceComponent;

	UPROPERTY(Transient)
	FAbilityCooldownState CooldownState;
	
	int32 FindRankIndex(FGameplayTag AbilityId) const;

	UPROPERTY(Transient)
	FAbilityRankState RankState;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|Granted", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<UAbility>> GrantedAbilityClasses;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UActiveAbility> ActiveAbility;

	UPROPERTY(Transient)
	TMap<FGameplayTag, int32> LooseOwnerTagCounts;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|Tags", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer ActiveGrantedTags;

	bool bAbilityTickEnabled = false;
	bool bDispatchingAbilityCallback = false;
	bool bResolvingBufferedInput = false;
	bool bEndingAbility = false;
	bool bResolveBufferedInputAfterCallback = false;
	
	void HandleInputPressed(FGameplayTag InputTag);
	void HandleInputReleased(FGameplayTag InputTag);
	
	UFUNCTION()
	void HandleModifierExpiry();

	void HandleModifiersChanged();
	void ScheduleModifierExpiry();
	double GetWorldTime() const;

	UPROPERTY(Transient)
	FStatModifierContainer ModifierContainer;

	FTimerHandle ModifierExpiryTimer;
	
	float ResolveFocusCost(const UActiveAbility* Ability) const;
	float ResolveCooldownDuration(const UActiveAbility* Ability) const;
};