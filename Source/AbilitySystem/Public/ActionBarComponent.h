#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActionBarComponent.generated.h"

class UAbility;
class UAbilityComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FActionBarChangedSignature, int32, SlotIndex);

/** One key on the action bar. */
USTRUCT(BlueprintType)
struct FActionBarSlotDefinition
{
	GENERATED_BODY()

	/** The input this slot fires, e.g. Input.Slot.1. Whatever ability sits here answers to it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	/** Shown on the slot: "1", "Q", "LMB". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar")
	FText KeybindLabel;

	/** Placed here at start, when learned (e.g. the basic attack on the LMB slot). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar")
	TSubclassOf<UAbility> StartingAbility;

	/** Newly learned abilities may land here by themselves. Off for mouse slots: only the player puts things there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action Bar")
	bool bAutoFill = true;
};

/**
 * The player's action bar: which ability sits on which key.
 *
 * Abilities with Place On Action Bar (the default) have no key of their own while this component exists:
 * they fire from the slot they're placed in, and not at all while unplaced. Abilities with it off
 * (basic attack, dodge) keep their own Activation Input Tag.
 *
 * Learning an ability puts it in the first empty slot (optional); unlearning removes it.
 * The bar widgets only display this and ask it for changes (drag and drop, right-click).
 */
UCLASS(ClassGroup = (Ability), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UActionBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UActionBarComponent();

	UFUNCTION(BlueprintPure, Category = "Action Bar")
	int32 GetNumSlots() const { return SlotDefinitions.Num(); }

	UFUNCTION(BlueprintPure, Category = "Action Bar")
	FActionBarSlotDefinition GetSlotDefinition(int32 SlotIndex) const;

	/** The ability in SlotIndex, or none. */
	UFUNCTION(BlueprintPure, Category = "Action Bar")
	TSubclassOf<UAbility> GetSlotAbility(int32 SlotIndex) const;

	/** Slot holding AbilityClass, or -1. */
	UFUNCTION(BlueprintPure, Category = "Action Bar")
	int32 FindSlotOfAbility(TSubclassOf<UAbility> AbilityClass) const;

	/** Granted, learned, an active ability, and meant for the bar. */
	UFUNCTION(BlueprintPure, Category = "Action Bar")
	bool CanPlaceAbility(TSubclassOf<UAbility> AbilityClass) const;

	/** Puts AbilityClass in SlotIndex. Already on the bar elsewhere = the two slots swap. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar")
	bool AssignAbility(int32 SlotIndex, TSubclassOf<UAbility> AbilityClass);

	/** Swaps two slots (dragging one slot onto another). */
	UFUNCTION(BlueprintCallable, Category = "Action Bar")
	bool MoveSlot(int32 FromSlotIndex, int32 ToSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Action Bar")
	bool ClearSlot(int32 SlotIndex);

	/** Right-click from a list: first empty slot. False when the bar is full or it can't be placed. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar")
	bool AddToFirstEmptySlot(TSubclassOf<UAbility> AbilityClass);

	UFUNCTION(BlueprintCallable, Category = "Action Bar")
	bool RemoveAbility(TSubclassOf<UAbility> AbilityClass);

	/** On the bar = removed, otherwise added to the first empty slot. */
	UFUNCTION(BlueprintCallable, Category = "Action Bar")
	bool ToggleAbility(TSubclassOf<UAbility> AbilityClass);

	UFUNCTION(BlueprintPure, Category = "Action Bar")
	UAbilityComponent* GetAbilityComponent() const { return AbilityComponent; }

	/** A slot changed. -1 = several or all of them. */
	UPROPERTY(BlueprintAssignable, Category = "Action Bar")
	FActionBarChangedSignature OnActionBarChanged;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The keys, left to right. Ten entries for a 1-0 bar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action Bar", meta = (TitleProperty = "KeybindLabel"))
	TArray<FActionBarSlotDefinition> SlotDefinitions;

	/** Placed at start in this order into the free auto-fill slots, when learned (after each slot's Starting Ability). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action Bar")
	TArray<TSubclassOf<UAbility>> StartingLayout;

	/** Learning an ability puts it in the first empty slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action Bar")
	bool bAutoPlaceLearnedAbilities = true;

private:
	UFUNCTION()
	void HandleAbilityRankChanged(FGameplayTag AbilityId, int32 OldRank, int32 NewRank);

	UFUNCTION()
	void HandleGrantedAbilitiesChanged();

	/** Pushes slot -> input to the ability component, so presses reach the right ability. */
	void ApplyInputBindings() const;

	void SetSlot(int32 SlotIndex, TSubclassOf<UAbility> AbilityClass);

	/** First empty slot that accepts automatic placement, or -1. */
	int32 FindFirstAutoFillSlot() const;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UAbility>> Slots;
};
