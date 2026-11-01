#pragma once

#include "CoreMinimal.h"
#include "ItemDataAsset.h"
#include "ModifierTypes.h"
#include "ResourceComponent.h"
#include "ConsumableDataAsset.generated.h"

/** Health or Focus given back the moment the item is used. Flat and percent add up. */
USTRUCT(BlueprintType)
struct FResourceRestore
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	EResourceType Resource = EResourceType::Health;

	/** Flat amount restored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "0.0"))
	float Amount = 0.0f;

	/** Percent of the resource's max restored. 25 = a quarter of Max Health. Scales with the character. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "Percent"))
	float PercentOfMax = 0.0f;
};

/**
 * An item used up from the bag, e.g. a health potion. Using one restores resources at once and
 * can add a timed buff, then starts a cooldown.
 *
 * Shared cooldowns: every consumable with the same Cooldown Group waits on the same cooldown,
 * wherever it sits in the bag. Give Minor and Greater Health Potion Cooldown.Item.Potion.Health
 * and drinking either one locks both; a Focus potion with its own group stays usable.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UConsumableDataAsset : public UItemDataAsset
{
	GENERATED_BODY()

public:
	UConsumableDataAsset();

	const TArray<FResourceRestore>& GetRestores() const { return Restores; }
	const TArray<FStatModifier>& GetStatModifiers() const { return StatModifiers; }
	const FGameplayTagContainer& GetGrantedTags() const { return GrantedTags; }

	UFUNCTION(BlueprintPure, Category = "Consumable")
	float GetEffectDuration() const { return EffectDuration; }

	/** True if using it adds a timed buff (modifiers or tags, with a duration). */
	UFUNCTION(BlueprintPure, Category = "Consumable")
	bool HasTimedEffect() const { return EffectDuration > 0.0f && (!StatModifiers.IsEmpty() || !GrantedTags.IsEmpty()); }

	UFUNCTION(BlueprintPure, Category = "Consumable")
	float GetCooldownDuration() const { return CooldownDuration; }

	/** The tag the cooldown is stored under: Cooldown Group, or this item's own Item Id when no group is set. */
	UFUNCTION(BlueprintPure, Category = "Consumable")
	FGameplayTag GetCooldownTag() const { return CooldownGroup.IsValid() ? CooldownGroup : ItemId; }

#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/** Given back the moment it is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Restore")
	TArray<FResourceRestore> Restores;

	/** Stat and attribute modifiers for Effect Duration after use. Read at rank 1. Shown on the active effects bar with the item's icon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effect")
	TArray<FStatModifier> StatModifiers;

	/** Owner tags for Effect Duration after use. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effect")
	FGameplayTagContainer GrantedTags;

	/** How long Stat Modifiers and Granted Tags last. 0 = no buff, only the restore. Using another refreshes it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Effect", meta = (ClampMin = "0.0", Units = "s"))
	float EffectDuration = 0.0f;

	/**
	 * Consumables with the same group share one cooldown (e.g. Cooldown.Item.Potion.Health).
	 * Empty = only stacks of this exact item share it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Cooldown", meta = (Categories = "Cooldown.Item"))
	FGameplayTag CooldownGroup;

	/** Seconds before anything in the group can be used again. 0 = no cooldown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Consumable|Cooldown", meta = (ClampMin = "0.0", Units = "s"))
	float CooldownDuration = 0.0f;
};
