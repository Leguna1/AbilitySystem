#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuestLogComponent.generated.h"

class APawn;
class UAdventureComponent;
class UAdventureSaveGame;
class UCombatantComponent;
class UExperienceComponent;
class UInventoryComponent;
class UQuestDataAsset;
struct FQuestObjective;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FQuestLogChangedSignature);

UENUM(BlueprintType)
enum class EQuestState : uint8
{
	/** Level too low, an earlier quest isn't finished, or a required world flag isn't set. */
	Unavailable,
	Available,
	Active,
	/** Every objective is done; hand it in at the villager who gave it. */
	ReadyToTurnIn,
	Completed
};

/** A quest the player is on, and what has been counted so far. */
USTRUCT(BlueprintType)
struct FActiveQuest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TObjectPtr<UQuestDataAsset> Quest;

	/** One per objective. Used by Kill and Talk To; Collect and World Flag are read live from the bag and the world. */
	UPROPERTY(BlueprintReadOnly, Category = "Quest")
	TArray<int32> Counts;
};

/**
 * The player's quests: which are active, how far along, which are finished.
 * Lives on the player controller; the Adventure Component creates it, nothing to set up.
 *
 * Counts by itself: kills credited to the player (the companion's too), items in the bag,
 * world flags, villagers talked to.
 */
UCLASS(ClassGroup = (Adventure))
class ABILITYSYSTEM_API UQuestLogComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuestLogComponent();

	UFUNCTION(BlueprintPure, Category = "Quests")
	EQuestState GetQuestState(const UQuestDataAsset* Quest) const;

	/** Takes the quest if it is Available. */
	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool AcceptQuest(UQuestDataAsset* Quest);

	/** Hands in a Ready To Turn In quest: takes collected items, gives the rewards, sets its flag. */
	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool TurnInQuest(UQuestDataAsset* Quest);

	/** Drops an active quest; its counts are lost. */
	UFUNCTION(BlueprintCallable, Category = "Quests")
	bool AbandonQuest(UQuestDataAsset* Quest);

	UFUNCTION(BlueprintPure, Category = "Quests")
	const TArray<FActiveQuest>& GetActiveQuests() const { return ActiveQuests; }

	UFUNCTION(BlueprintPure, Category = "Quests")
	int32 GetCompletedQuestCount() const { return CompletedQuests.Num(); }

	/** How far objective ObjectiveIndex of an active quest is, capped at what it needs. 0 if the quest isn't active. */
	UFUNCTION(BlueprintPure, Category = "Quests")
	int32 GetObjectiveCount(const UQuestDataAsset* Quest, int32 ObjectiveIndex) const;

	/** Called by a villager when the player talks to it; completes Talk To objectives for NpcId. */
	UFUNCTION(BlueprintCallable, Category = "Quests")
	void NotifyTalkedTo(FName NpcId);

	/** "Title" then one line per objective with its count, for every active quest. For the HUD tracker. */
	FText BuildTrackerText() const;

	/** The objectives of Quest as lines, with counts when it is active. For the quest's page in a dialogue. */
	FText BuildObjectivesText(const UQuestDataAsset* Quest) const;

	/** A quest was taken, advanced, handed in or dropped. */
	UPROPERTY(BlueprintAssignable, Category = "Quests")
	FQuestLogChangedSignature OnQuestLogChanged;

	/** The Adventure Component hands over the player's pawn (and again whenever it changes). */
	void SetPlayerPawn(APawn* NewPawn);

	void WriteSaveData(UAdventureSaveGame& Save) const;
	void ApplySaveData(const UAdventureSaveGame& Save);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleKill(AActor* Victim);

	UFUNCTION()
	void HandleInventorySlotChanged(int32 SlotIndex);

	UFUNCTION()
	void HandleFlagSet(FName Flag);

	int32 FindActiveQuestIndex(const UQuestDataAsset* Quest) const;
	int32 GetObjectiveCountInternal(const FActiveQuest& ActiveQuest, int32 ObjectiveIndex) const;
	bool AreObjectivesComplete(const FActiveQuest& ActiveQuest) const;
	int32 GetPlayerLevel() const;
	void UnbindFromPawn();

	/** Tells the player when a quest that wasn't ready before the change is ready now. */
	void AnnounceNewlyReady(const TArray<bool>& WasReady);
	void GetReadyStates(TArray<bool>& OutReady) const;

	UAdventureComponent* GetAdventure() const;

	UPROPERTY(Transient)
	TArray<FActiveQuest> ActiveQuests;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UQuestDataAsset>> CompletedQuests;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryComponent> Inventory;

	UPROPERTY(Transient)
	TObjectPtr<UExperienceComponent> ExperienceComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;
};
