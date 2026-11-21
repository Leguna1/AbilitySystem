#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EnhancementComponent.generated.h"

class UAdventureSaveGame;
class UCombatantComponent;
class UEquipmentDataAsset;
class UInventoryComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnhancementChangedSignature, FGameplayTag, SlotTag, int32, NewLevel);

UENUM(BlueprintType)
enum class EEnhanceResult : uint8
{
	/** The slot went up a level. */
	Success,
	/** The attempt failed: gold and materials are gone, the level stays (or drops, with Downgrade On Failure). */
	Failed,
	/** Nothing was spent: max level, nothing worn in the slot, or not enough gold or materials. */
	NotPossible
};

/** Stands in as the source of one slot's bonus effect, so each slot's bonus can be replaced on its own. */
UCLASS()
class ABILITYSYSTEM_API UEnhancementSlotSource : public UObject
{
	GENERATED_BODY()
};

/**
 * Gear enhancement, the blacksmith's trade. Each equipment slot (Weapon, Chest, Feet...) has a level, +0 to
 * Max Enhancement Level. Whatever is worn in the slot gets its own stats again, times Level x Bonus Per Level:
 * a bow with +20 Attack Power in a +3 slot gives +26 at 10% per level.
 *
 * The level belongs to the slot, not the item, so new loot is always worth wearing: swapping a bow in
 * keeps the +3. Raising a level costs gold (and a material, if one is set) and can fail.
 *
 * Lives on the player's pawn; the Adventure Component adds it by itself. Numbers are in
 * Project Settings -> Adventure -> Enhancement.
 */
UCLASS(ClassGroup = (Adventure), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UEnhancementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnhancementComponent();

	UFUNCTION(BlueprintPure, Category = "Enhancement")
	int32 GetEnhancementLevel(FGameplayTag SlotTag) const;

	/** Gold for the slot's next level. 0 at max level. */
	UFUNCTION(BlueprintPure, Category = "Enhancement")
	int32 GetNextLevelCost(FGameplayTag SlotTag) const;

	/** Percent chance (0..100) that the slot's next level succeeds. */
	UFUNCTION(BlueprintPure, Category = "Enhancement")
	float GetNextLevelChance(FGameplayTag SlotTag) const;

	/** True when an attempt can be paid for and the slot has something worn below max level. */
	UFUNCTION(BlueprintPure, Category = "Enhancement")
	bool CanEnhance(FGameplayTag SlotTag) const;

	/** Pays and rolls. */
	UFUNCTION(BlueprintCallable, Category = "Enhancement")
	EEnhanceResult TryEnhance(FGameplayTag SlotTag);

	/** Sets a slot's level directly (loading a game, cheats). */
	UFUNCTION(BlueprintCallable, Category = "Enhancement")
	void SetEnhancementLevel(FGameplayTag SlotTag, int32 NewLevel);

	UPROPERTY(BlueprintAssignable, Category = "Enhancement")
	FEnhancementChangedSignature OnEnhancementChanged;

	void WriteSaveData(UAdventureSaveGame& Save) const;
	void ApplySaveData(const UAdventureSaveGame& Save);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** A slot's level and the object its bonus effect is applied under. */
	struct FSlotState
	{
		FGameplayTag SlotTag;
		int32 Level = 0;
		TWeakObjectPtr<UEnhancementSlotSource> Source;
	};

	UFUNCTION()
	void HandleEquipmentChanged(int32 EquipmentSlotIndex, UEquipmentDataAsset* PreviousItem, UEquipmentDataAsset* NewItem);

	int32 FindSlotState(FGameplayTag SlotTag) const;
	FSlotState& FindOrAddSlotState(FGameplayTag SlotTag);

	/** Removes the slot's bonus and applies it again for what is worn there now. */
	void RefreshSlotBonus(FGameplayTag SlotTag);

	TArray<FSlotState> SlotStates;

	/** Keeps the source objects alive; Slot States only point at them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEnhancementSlotSource>> SlotSources;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;
};
