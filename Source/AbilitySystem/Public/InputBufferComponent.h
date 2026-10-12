#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AbilityComponent.h"
#include "InputBufferTypes.h"
#include "InputBufferComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputBufferTagEventSignature, FGameplayTag, InputTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInputBufferClearedEventSignature);

/**
 * The player's bridge from input to abilities. The ability system itself knows nothing about input
 * timing; this component decides when a press becomes an activation request.
 *
 * - Remembers presses for a short time (buffering) and which inputs are held.
 * - Tells the active ability about presses and releases (hold to charge, release to fire).
 * - Requests abilities with TryActivateAbility: immediately when possible, otherwise again whenever
 *   the ability component announces an activation window (ability ended, combo checkpoint, transition).
 *
 * Only the player needs one. AI calls TryActivateAbility directly.
 * Which input starts which ability, and how long it's remembered, is read from each ability's Input settings.
 */
UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UInputBufferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInputBufferComponent();

	/* -------------------- Entry points (call these from input events) -------------------- */

	/** Single entry point for ability input: true on press (Started), false on release (Completed). */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void HandleInput(FGameplayTag InputTag, bool bPressed);

	/** Movement for abilities that read it (e.g. directional dodge). */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer|Movement")
	void HandleMovementInput(FVector2D MovementInput);

	/** Tries waiting presses (and held inputs) against the granted abilities, best first. True if one started. */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	bool ResolveBufferedInputs();

	/* -------------------- Storage -------------------- */

	/**
	 * Marks InputTag as held and creates a buffered press entry.
	 * A negative BufferDurationOverride uses DefaultBufferDuration.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	bool InputPressed(FGameplayTag InputTag, float BufferDurationOverride = -1.0f);

	/** Removes InputTag from the held state. Its buffered press stays valid until consumed or expired. */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	bool InputReleased(FGameplayTag InputTag);

	UFUNCTION(BlueprintPure, Category = "Input Buffer")
	bool IsInputHeld(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category = "Input Buffer")
	bool HasBufferedInput(FGameplayTag InputTag) const;

	/**
	 * All currently valid buffered presses. With bIncludeHeldInputs, also a Held candidate
	 * for each held tag that has no remaining Pressed entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void GetInputCandidates(TArray<FBufferedInput>& OutCandidates, bool bIncludeHeldInputs = true);

	/** Consumes a candidate after it started an ability. Consuming a Held candidate does not release the input. */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	bool ConsumeInput(const FBufferedInput& Input);

	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void ClearBufferedInput(FGameplayTag InputTag);

	/** Clears buffered presses without changing held state. */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void ClearBufferedInputs();

	/** Clears buffered presses and held state. */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void ResetInputBuffer();

	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void PruneExpiredInputs();

	UFUNCTION(BlueprintPure, Category = "Input Buffer")
	int32 GetBufferedInputCount() const { return BufferedInputs.Num(); }

	UFUNCTION(BlueprintPure, Category = "Input Buffer")
	const FGameplayTagContainer& GetHeldInputTags() const { return HeldInputTags; }

	UFUNCTION(BlueprintCallable, Category = "Input Buffer|Movement")
	void SetMovementInput(FVector2D MovementInput);

	UFUNCTION(BlueprintPure, Category = "Input Buffer|Movement")
	FVector2D GetMovementInput() const { return CurrentMovementInput; }

	UPROPERTY(BlueprintAssignable, Category = "Input Buffer|Events")
	FInputBufferTagEventSignature InputPressedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Input Buffer|Events")
	FInputBufferTagEventSignature InputReleasedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Input Buffer|Events")
	FInputBufferClearedEventSignature InputBufferClearedEvent;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Buffer|Movement", meta = (AllowPrivateAccess = "true"))
	FVector2D CurrentMovementInput = FVector2D::ZeroVector;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FInputActivationCandidate
	{
		FGameplayTag AbilityId;
		FBufferedInput BufferedInput;
		int32 Priority = 0;
	};

	/** Granted, learned abilities whose input matches a waiting press or held input, best first. */
	void BuildActivationCandidates(TArray<FInputActivationCandidate>& OutCandidates);

	/** Whether a fresh press should be tried right away, or wait for the next activation window. */
	bool ShouldResolveOnPress(FGameplayTag InputTag) const;

	/** How long a press is remembered: the longest Input Buffer Duration among abilities on that input. */
	float GetLongestBufferDurationForInput(FGameplayTag InputTag) const;

	UFUNCTION()
	void HandleActivationWindowEvent(EActivationWindowEvent WindowEvent);

	float GetCurrentTime() const;
	float ResolveBufferDuration(float BufferDurationOverride) const;
	bool HasPressedEntry(FGameplayTag InputTag) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Buffer", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DefaultBufferDuration = 0.25f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Buffer", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer HeldInputTags;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Buffer", meta = (AllowPrivateAccess = "true"))
	TArray<FBufferedInput> BufferedInputs;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	TMap<FGameplayTag, uint64> HeldInputSequences;
	uint64 NextSequence = 1;

	/** Guards against re-entry: an activation can open a window while we're resolving. */
	bool bResolving = false;
};
