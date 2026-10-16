#pragma once

#include "CoreMinimal.h"
#include "ActiveAbility.h"
#include "AbilityTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectKey.h"
#include "HitReactionTypes.h"
#include "ResourceComponent.h"
#include "AbilityComponent.generated.h"

class ACharacter;
class UAbility;
class UMotionWarpingComponent;
class UResourceComponent;
class UTargetingComponent;
class UAnimSequenceBase;
class UCombatantComponent;
class UPassiveAbility;
class UHitReactionAbility;
class UAttributeComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAbilityActivatedEventSignature, FGameplayTag, AbilityId, UActiveAbility*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAbilityCommittedEventSignature, FGameplayTag, AbilityId, UActiveAbility*, Ability);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAbilityEndedEventSignature, FGameplayTag, AbilityId, UActiveAbility*, Ability, EAbilityEndReason, EndReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOwnedTagsChangedEventSignature, FGameplayTagContainer, OwnedTags);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGrantedAbilitiesChangedEventSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAbilityRankChangedEventSignature, FGameplayTag, AbilityId, int32, OldRank, int32, NewRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FModifiersChangedEventSignature);

/** Activation timing news for whoever requests abilities on a schedule (the player's input buffer). */
UENUM(BlueprintType)
enum class EActivationWindowEvent : uint8
{
	/** A new activation may succeed now (an ability ended, a transition or input checkpoint opened): retry waiting requests. */
	Opened,

	/** Waiting requests must be dropped (staggered, killed): presses made before must not fire afterwards. */
	Flushed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActivationWindowEventSignature, EActivationWindowEvent, WindowEvent);

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

	/**
	 * Requests an ability by id. The one way abilities start, for the player's input buffer and AI alike.
	 * With an ability already active: the same ability runs its next step if it handles repeats (combos),
	 * otherwise the request replaces the active one when transition, early-cancel or interrupt rules allow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	bool TryActivateAbility(FGameplayTag AbilityId);

	/**
	 * Cancels the active ability. bOpenActivationWindow = false for death, stuns and hit reactions,
	 * so a buffered request can't fire straight through them.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	void CancelActiveAbility(bool bOpenActivationWindow = true);

	/** Activation timing for schedulers such as the player's input buffer. AI doesn't need it. */
	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FActivationWindowEventSignature ActivationWindowEvent;

	/* -------------------- Input notifications -------------------- */
	// Tells the active ability about input (hold to charge, release to fire). No buffering happens here.

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void NotifyInputPressed(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void NotifyInputReleased(FGameplayTag InputTag);

	UFUNCTION(BlueprintCallable, Category = "Ability|Input")
	void MovementInputReceived(FVector2D MovementInput);

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	bool IsInputHeld(FGameplayTag InputTag) const;

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
	
	/**
 * Shortens the remaining cooldown of every ability whose id matches AbilityIds
 * (parents match too, unless bExactMatch). RemainingFraction first (0.25 = a quarter off), then Seconds.
 * Returns how many cooldowns changed.
 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Cooldown")
	int32 ReduceCooldowns(const FGameplayTagContainer& AbilityIds, float Seconds, float RemainingFraction = 0.0f, bool bExactMatch = false);

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

	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability|References")
	UMotionWarpingComponent* GetMotionWarpingComponent() const { return MotionWarpingComponent; }
	
	

	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	FVector2D GetMovementInput() const { return CurrentMovementInput; }

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
	
	/** Forwards to the combatant component, where every effect on this character lives. */
	UFUNCTION(BlueprintPure, Category = "Ability|Modifiers")
	float GetModifiedValue(FGameplayTag Stat, float BaseValue, const FGameplayTagContainer& AbilityTags) const;

	/** Relays the combatant's OnEffectsChanged for ability-side listeners (e.g. hotbar cost display). */
	UPROPERTY(BlueprintAssignable, Category = "Ability|Events")
	FModifiersChangedEventSignature ModifiersChangedEvent;
	
	/** Focus cost per payment, including modifiers. What the hotbar shows and what a payment spends. */
	UFUNCTION(BlueprintPure, Category = "Ability|Cost")
	float GetAbilityFocusCost(FGameplayTag AbilityId) const;
	
	/** Forwards a gameplay event from an ability to every active passive. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Events")
	void BroadcastGameplayEvent(FGameplayTag EventTag, UActiveAbility* Source);
	
	/** Interrupts the active ability and plays the hit reaction. Stagger or stronger only. */
	UFUNCTION(BlueprintCallable, Category = "Ability|Hit Reaction")
	bool TriggerHitReaction(const FHitReactionResult& Reaction);

	/** The reaction being started; read by UHitReactionAbility when it activates. */
	UFUNCTION(BlueprintPure, Category = "Ability|Hit Reaction")
	const FHitReactionResult& GetPendingHitReaction() const { return PendingHitReaction; }
	
	UFUNCTION(BlueprintPure, Category = "Ability")
	UCombatantComponent* GetCombatantComponent() const { return CombatantComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	UAttributeComponent* GetAttributeComponent() const { return AttributeComponent; }
	
	/**
 * Drops anything waiting to activate (e.g. the player's buffered presses). For moments that make
 * earlier requests stale: death, a stagger, a weapon swap.
 */
	UFUNCTION(BlueprintCallable, Category = "Ability")
	void FlushActivationRequests();
	
	/* -------------------- Input binding -------------------- */

	/**
	 * The input that starts AbilityId: its action bar slot when placed there. Without an action bar,
	 * or for abilities that don't use it, the ability's own Activation Input Tag. None = can't be pressed.
	 */
	UFUNCTION(BlueprintPure, Category = "Ability|Input")
	FGameplayTag GetAbilityInputTag(FGameplayTag AbilityId) const;

	/** Set by the Action Bar Component. */
	void SetActionBarDrivesInput(bool bDrives) { bActionBarDrivesInput = bDrives; }
	void SetAbilityInputOverride(FGameplayTag AbilityId, FGameplayTag InputTag);
	void ClearAbilityInputOverrides() { AbilityInputOverrides.Reset(); }

protected:
	virtual void BeginPlay() override;
	virtual void InitializeComponent() override;
	
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	
	/** Played when this character is staggered. Granted automatically. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Hit Reaction")
	TSubclassOf<UHitReactionAbility> HitReactionAbilityClass;

private:
	bool CommitAbility(UActiveAbility* RequestingAbility);
	void EndAbility(UActiveAbility* RequestingAbility, EAbilityEndReason EndReason);
	void SetAbilityTickEnabled(UActiveAbility* RequestingAbility, bool bEnabled);
	void SetAbilityTransitionOpen(UActiveAbility* RequestingAbility, bool bOpen);
	void SetAbilityEarlyCancellationClosed(UActiveAbility* RequestingAbility, bool bClosed) const;

	/** The active ability says "a follow-up may start now" (e.g. a combo's input checkpoint). */
	void RequestActivationWindow(UActiveAbility* RequestingAbility);

	/** Broadcasts Opened now, or right after the current ability callback returns. */
	void OpenActivationWindow();
	void BroadcastActivationWindow(EActivationWindowEvent WindowEvent);

	bool ExecuteRepeatedAbilityRequest(UActiveAbility* Ability);
	bool ReplaceActiveAbility(TSubclassOf<UAbility> IncomingAbilityClass);

	bool CanActivateAbilityInstance(const UActiveAbility* Ability, bool bReplacingActiveAbility) const;
	static bool CanReplaceActiveAbility(const UActiveAbility* CurrentAbility, const UActiveAbility* IncomingAbility, EAbilityEndReason& OutReplacementReason);
	/** Null for passive classes: only active abilities have executions. */
	UActiveAbility* CreateExecutionInstance(TSubclassOf<UAbility> AbilityClass);
	bool ActivateAbilityInstance(UActiveAbility* Ability);
	void EndActiveAbilityInternal(EAbilityEndReason EndReason, bool bOpenActivationWindow = true);

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
	bool bEndingAbility = false;
	bool bOpenWindowAfterCallback = false;

	/** Inputs currently held, as told by NotifyInputPressed/Released. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|Input", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer HeldInputTags;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Ability|Input", meta = (AllowPrivateAccess = "true"))
	FVector2D CurrentMovementInput = FVector2D::ZeroVector;
	
	double GetWorldTime() const;
	
	float ResolveFocusCost(const UActiveAbility* Ability) const;
	float ResolveCooldownDuration(const UActiveAbility* Ability) const;
	
	/* -------------------- Passives -------------------- */

	UPassiveAbility* FindPassiveInstance(TSubclassOf<UAbility> AbilityClass) const;
	void CreatePassiveInstance(TSubclassOf<UAbility> AbilityClass);
	void DestroyPassiveInstance(TSubclassOf<UAbility> AbilityClass);

	/** Re-snapshots the passive's rank and re-applies its modifiers at the new rank. */
	void RefreshPassiveRank(TSubclassOf<UAbility> AbilityClass);

	/** Calls Func on every active passive. Iterates a copy, so passives may grant or revoke abilities from inside. */
	void ForEachActivePassive(TFunctionRef<void(UPassiveAbility*)> Func);

	UFUNCTION()
	void HandleOwnerDamageDealt(AActor* Target, const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandleOwnerPayloadReceived(const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandleOwnerKilled(AActor* Victim);

	UFUNCTION()
	void HandleOwnerDied(AActor* Killer);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPassiveAbility>> PassiveInstances;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;
	
	UFUNCTION()
	void HandleOwnerHitReaction(const FHitReactionResult& Result);

	UPROPERTY(Transient)
	FHitReactionResult PendingHitReaction;
	
	UFUNCTION()
	void HandleCombatantEffectsChanged();
	
	UPROPERTY(Transient)
	TObjectPtr<UAttributeComponent> AttributeComponent;
	
	UFUNCTION()
    void HandleOwnerResourceChanged(EResourceType ResourceType, EResourceValueType ValueType, float OldValue, float NewValue);
	
	/** Ability id -> the action bar slot's input. */
	TMap<FGameplayTag, FGameplayTag> AbilityInputOverrides;

	bool bActionBarDrivesInput = false;
	
};