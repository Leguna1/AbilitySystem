#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "InputBufferTypes.h"
#include "InputBufferComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInputBufferTagEventSignature, FGameplayTag, InputTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInputBufferClearedEventSignature);

/**
 * Stores raw tagged player input independently from the ability system.
 *
 * It owns:
 * - currently held input tags
 * - short-lived pressed-input entries
 * - input timestamps and ordering
 * - input expiration
 * - input consumption
 *
 * It does not know:
 * - which abilities exist
 * - ability priority
 * - activation requirements
 * - whether an ability may interrupt another
 */
UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UInputBufferComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInputBufferComponent();

	/**
	 * Marks InputTag as held and creates a buffered press entry.
	 *
	 * A negative BufferDurationOverride uses DefaultBufferDuration.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	bool InputPressed(FGameplayTag InputTag, float BufferDurationOverride = -1.0f);

	/**
	 * Removes InputTag from the held-input state.
	 *
	 * Its existing buffered press remains valid until consumed or expired.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	bool InputReleased(FGameplayTag InputTag);

	UFUNCTION(BlueprintPure, Category = "Input Buffer")
	bool IsInputHeld(FGameplayTag InputTag) const;

	UFUNCTION(BlueprintPure, Category = "Input Buffer")
	bool HasBufferedInput(FGameplayTag InputTag) const;

	/**
	 * Returns all currently valid buffered presses.
	 *
	 * When bIncludeHeldInputs is true, a synthetic Held candidate is also
	 * returned for each held tag that has no remaining Pressed entry.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void GetInputCandidates(TArray<FBufferedInput>& OutCandidates, bool bIncludeHeldInputs = true);

	/**
	 * Consumes a candidate after an ability successfully executes it.
	 *
	 * Consuming a Held candidate does not release the physical input.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	bool ConsumeInput(const FBufferedInput& Input);

	/** Removes all buffered press entries for InputTag. */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void ClearBufferedInput(FGameplayTag InputTag);

	/** Clears buffered press entries without changing held-input state. */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void ClearBufferedInputs();

	/** Clears both buffered entries and held-input state. */
	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void ResetInputBuffer();

	UFUNCTION(BlueprintCallable, Category = "Input Buffer")
	void PruneExpiredInputs();

	UFUNCTION(BlueprintPure, Category = "Input Buffer")
	int32 GetBufferedInputCount() const { return BufferedInputs.Num(); }

	UFUNCTION(BlueprintPure, Category = "Input Buffer")
	const FGameplayTagContainer& GetHeldInputTags() const { return HeldInputTags; }

	UPROPERTY(BlueprintAssignable, Category = "Input Buffer|Events")
	FInputBufferTagEventSignature InputPressedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Input Buffer|Events")
	FInputBufferTagEventSignature InputReleasedEvent;

	UPROPERTY(BlueprintAssignable, Category = "Input Buffer|Events")
	FInputBufferClearedEventSignature InputBufferClearedEvent;
	
	UFUNCTION(BlueprintCallable, Category = "Input Buffer|Movement")
	void SetMovementInput(FVector2D MovementInput);

	UFUNCTION(BlueprintPure, Category = "Input Buffer|Movement")
	FVector2D GetMovementInput() const { return CurrentMovementInput; }
	
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Buffer|Movement", meta = (AllowPrivateAccess = "true"))
	FVector2D CurrentMovementInput = FVector2D::ZeroVector;

protected:
	virtual void BeginPlay() override;

private:
	float GetCurrentTime() const;
	float ResolveBufferDuration(float BufferDurationOverride) const;
	bool HasPressedEntry(FGameplayTag InputTag) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Buffer", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float DefaultBufferDuration = 0.25f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Buffer", meta = (AllowPrivateAccess = "true"))
	FGameplayTagContainer HeldInputTags;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Input Buffer", meta = (AllowPrivateAccess = "true"))
	TArray<FBufferedInput> BufferedInputs;

	TMap<FGameplayTag, uint64> HeldInputSequences;
	uint64 NextSequence = 1;
};