#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "AdventureSettings.generated.h"

class UCurrencyDataAsset;
class UItemDataAsset;

/**
 * Project Settings -> Game -> Adventure. Everything the game layer needs that isn't tied to one actor:
 * keys, the gold currency, death and respawn, gear enhancement, saving.
 * Works with its defaults; the one thing worth setting is Gold Currency (your DA_Gold).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Adventure"))
class ABILITYSYSTEM_API UAdventureSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAdventureSettings();

	static const UAdventureSettings* Get() { return GetDefault<UAdventureSettings>(); }

	/* -------------------- Input -------------------- */

	/** Talk, open, pull, cleanse: whatever the thing in front of the player does. */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	FKey InteractKey = EKeys::F;

	/** Opens and closes the quest log. */
	UPROPERTY(Config, EditAnywhere, Category = "Input")
	FKey QuestLogKey = EKeys::L;

	/** How close (to its collision bounds) the player must be to interact with something. */
	UPROPERTY(Config, EditAnywhere, Category = "Input", meta = (ClampMin = "50.0", Units = "cm"))
	float InteractionRadius = 220.0f;

	/* -------------------- Economy -------------------- */

	/** The currency quests, shops, chests and the blacksmith use (DA_Gold). Empty = no gold is given or asked for. */
	UPROPERTY(Config, EditAnywhere, Category = "Economy")
	TSoftObjectPtr<UCurrencyDataAsset> GoldCurrency;

	/* -------------------- Death -------------------- */

	/** Seconds between the player's death and waking up at the last waystone. */
	UPROPERTY(Config, EditAnywhere, Category = "Death", meta = (ClampMin = "0.5", Units = "s"))
	float RespawnDelay = 5.0f;

	/** Health the player wakes up with, as a fraction of max. */
	UPROPERTY(Config, EditAnywhere, Category = "Death", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float RespawnHealthFraction = 1.0f;

	/** Fraction of carried gold lost on death. 0 = dying is free. */
	UPROPERTY(Config, EditAnywhere, Category = "Death", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeathGoldLossFraction = 0.05f;

	/* -------------------- Enhancement -------------------- */

	/** Highest level an equipment slot can be enhanced to. */
	UPROPERTY(Config, EditAnywhere, Category = "Enhancement", meta = (ClampMin = "0", ClampMax = "30"))
	int32 MaxEnhancementLevel = 10;

	/** Each level adds this fraction of the worn item's own stats (0.1 = +10% per level, so +10 doubles the item). */
	UPROPERTY(Config, EditAnywhere, Category = "Enhancement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EnhancementBonusPerLevel = 0.1f;

	/** Gold for the first level. Level N costs Base x N ^ Growth. */
	UPROPERTY(Config, EditAnywhere, Category = "Enhancement", meta = (ClampMin = "0"))
	int32 EnhancementBaseCost = 50;

	UPROPERTY(Config, EditAnywhere, Category = "Enhancement", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float EnhancementCostGrowth = 1.6f;

	/** Item spent on every attempt, on top of the gold (an enhancement stone that monsters drop). Empty = gold only. */
	UPROPERTY(Config, EditAnywhere, Category = "Enhancement")
	TSoftObjectPtr<UItemDataAsset> EnhancementMaterial;

	UPROPERTY(Config, EditAnywhere, Category = "Enhancement", meta = (ClampMin = "1"))
	int32 EnhancementMaterialsPerAttempt = 1;

	/**
	 * Percent chance to succeed, one entry per target level (+1 first). Levels past the end use the last entry.
	 * A failed attempt only loses the gold and materials, unless Downgrade On Failure is on.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Enhancement")
	TArray<float> EnhancementSuccessChance;

	/** A failed attempt also lowers the slot by one level, from Downgrade From Level up. For a harsher game. */
	UPROPERTY(Config, EditAnywhere, Category = "Enhancement")
	bool bDowngradeOnFailure = false;

	UPROPERTY(Config, EditAnywhere, Category = "Enhancement", meta = (ClampMin = "1", EditCondition = "bDowngradeOnFailure"))
	int32 DowngradeFromLevel = 6;

	/* -------------------- Saving -------------------- */

	/**
	 * Progress (altars, bosses, chests, quests, level, bag, gold) is saved at waystones, on every altar, boss and
	 * finished quest, and every Autosave Interval. Off while testing if you want every play session to start fresh.
	 * Console: Adventure.ResetSave deletes the save.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Saving")
	bool bEnableSaving = true;

	UPROPERTY(Config, EditAnywhere, Category = "Saving")
	FString SaveSlotName = TEXT("Adventure");

	/** Seconds between automatic saves. 0 = only at the moments listed above. */
	UPROPERTY(Config, EditAnywhere, Category = "Saving", meta = (ClampMin = "0.0", Units = "s"))
	float AutosaveInterval = 120.0f;
};
