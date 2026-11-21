#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "AdventureSaveGame.generated.h"

/** A stack of one item, by asset path so the save doesn't depend on loaded objects. */
USTRUCT()
struct FSavedItemStack
{
	GENERATED_BODY()

	UPROPERTY()
	FSoftObjectPath Item;

	UPROPERTY()
	int32 Count = 0;
};

USTRUCT()
struct FSavedQuest
{
	GENERATED_BODY()

	UPROPERTY()
	FSoftObjectPath Quest;

	/** One entry per objective (kills counted, villagers talked to). Unused for finished quests. */
	UPROPERTY()
	TArray<int32> Counts;

	UPROPERTY()
	bool bCompleted = false;
};

/** A gameplay tag and a number: an equipment slot's enhancement level, points in a core stat, a skill node's rank. */
USTRUCT()
struct FSavedTagValue
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 Value = 0;
};

/**
 * Everything the game remembers between sessions. Written by the World Progress Subsystem (the world's flags)
 * and the Adventure Component (the player). One slot; see Project Settings -> Adventure -> Saving.
 */
UCLASS()
class ABILITYSYSTEM_API UAdventureSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	/* -------------------- World -------------------- */

	/** Altars taken, bosses defeated, chests opened, gates unlocked, zones discovered, waystones attuned. */
	UPROPERTY()
	TArray<FName> Flags;

	/* -------------------- Player -------------------- */

	/** False = the save holds only world flags (no player was around when it was written). */
	UPROPERTY()
	bool bHasPlayerData = false;

	UPROPERTY()
	int32 Level = 1;

	UPROPERTY()
	int32 Experience = 0;

	UPROPERTY()
	bool bHasCheckpoint = false;

	UPROPERTY()
	FTransform Checkpoint;

	/** The level the checkpoint is in. Loading in another level keeps everything but the position. */
	UPROPERTY()
	FString CheckpointLevel;

	UPROPERTY()
	TArray<FSavedItemStack> BagItems;

	UPROPERTY()
	TArray<FSoftObjectPath> EquippedItems;

	UPROPERTY()
	int32 Gold = 0;

	UPROPERTY()
	TArray<FSavedTagValue> EnhancementLevels;

	UPROPERTY()
	TArray<FSavedTagValue> CoreStatPoints;

	UPROPERTY()
	TArray<FSavedTagValue> SkillNodeRanks;

	UPROPERTY()
	TArray<FSavedQuest> Quests;
};
