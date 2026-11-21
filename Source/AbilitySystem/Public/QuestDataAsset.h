#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "InventoryComponent.h"
#include "QuestDataAsset.generated.h"

class UItemDataAsset;

/** What a quest objective counts. */
UENUM(BlueprintType)
enum class EQuestObjectiveType : uint8
{
	/** Kill Required Count enemies of Target Class (children count too: BP_Golem_Elite counts for BP_Golem). */
	Kill UMETA(DisplayName = "Kill"),

	/** Have Required Count of Item in the bag when turning in. */
	Collect UMETA(DisplayName = "Collect"),

	/** A world flag is set: an altar taken ("Altar.Dawn"), a boss dead, a chest opened, a zone discovered. */
	WorldFlag UMETA(DisplayName = "World Flag"),

	/** Talk to the villager whose Npc Id is this. */
	TalkTo UMETA(DisplayName = "Talk To")
};

USTRUCT(BlueprintType)
struct FQuestObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	EQuestObjectiveType Type = EQuestObjectiveType::Kill;

	/** Shown in the tracker before the count, e.g. "Stone golems destroyed". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "Type == EQuestObjectiveType::Kill", EditConditionHides))
	TSubclassOf<AActor> TargetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "Type == EQuestObjectiveType::Collect", EditConditionHides))
	TObjectPtr<UItemDataAsset> Item;

	/** The same name as the altar's Altar Id, the boss's Boss Id, the chest's Chest Id, the zone's Zone Id... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "Type == EQuestObjectiveType::WorldFlag", EditConditionHides))
	FName Flag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (EditCondition = "Type == EQuestObjectiveType::TalkTo", EditConditionHides))
	FName NpcId;

	/** How many kills or items. World Flag and Talk To are always 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Objective", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	int32 GetRequiredCount() const
	{
		return Type == EQuestObjectiveType::Kill || Type == EQuestObjectiveType::Collect ? FMath::Max(RequiredCount, 1) : 1;
	}
};

/**
 * One quest: who can take it, what to do, what it pays. Make one data asset per quest
 * (right click -> Miscellaneous -> Data Asset -> Quest Data Asset) and add it to a villager's Quests list.
 * The quest is taken from and handed in to that villager.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UQuestDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest")
	FText Title;

	/** What the villager says when offering it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = "true"))
	FText Description;

	/** What the villager says when it is handed in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest", meta = (MultiLine = "true"))
	FText CompletionText;

	/* -------------------- Requirements -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Requirements", meta = (ClampMin = "1"))
	int32 RequiredLevel = 1;

	/** Quests that must be finished first (a quest chain). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Requirements")
	TArray<TObjectPtr<UQuestDataAsset>> Prerequisites;

	/** World flags that must be set first, e.g. "Altar.Dawn" for a quest that only makes sense after that altar. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Requirements")
	TArray<FName> RequiredFlags;

	/* -------------------- Objectives -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Objectives", meta = (TitleProperty = "Description"))
	TArray<FQuestObjective> Objectives;

	/** Collect objectives take their items from the bag when the quest is handed in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Objectives")
	bool bConsumeCollectedItems = true;

	/* -------------------- Rewards -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Rewards", meta = (ClampMin = "0"))
	int32 ExperienceReward = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Rewards", meta = (ClampMin = "0"))
	int32 GoldReward = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Rewards")
	TArray<FInventorySlot> ItemRewards;

	/** World flag set when the quest is handed in. A gate with this in Required Flags opens: quests can unlock places. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Quest|Rewards")
	FName FlagOnComplete;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
