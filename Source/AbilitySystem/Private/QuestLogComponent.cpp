#include "QuestLogComponent.h"

#include "AdventureComponent.h"
#include "AdventureSaveGame.h"
#include "CombatantComponent.h"
#include "ExperienceComponent.h"
#include "GameFramework/Pawn.h"
#include "InventoryComponent.h"
#include "ItemDataAsset.h"
#include "QuestDataAsset.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "QuestLog"

UQuestLogComponent::UQuestLogComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UQuestLogComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->OnFlagSet.AddUniqueDynamic(this, &UQuestLogComponent::HandleFlagSet);
	}
}

void UQuestLogComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->OnFlagSet.RemoveDynamic(this, &UQuestLogComponent::HandleFlagSet);
	}

	UnbindFromPawn();
	Super::EndPlay(EndPlayReason);
}

UAdventureComponent* UQuestLogComponent::GetAdventure() const
{
	return IsValid(GetOwner()) ? GetOwner()->FindComponentByClass<UAdventureComponent>() : nullptr;
}

/* -------------------- Player pawn -------------------- */

void UQuestLogComponent::SetPlayerPawn(APawn* NewPawn)
{
	UnbindFromPawn();

	if (!IsValid(NewPawn))
	{
		return;
	}

	Inventory = NewPawn->FindComponentByClass<UInventoryComponent>();
	ExperienceComponent = NewPawn->FindComponentByClass<UExperienceComponent>();
	CombatantComponent = NewPawn->FindComponentByClass<UCombatantComponent>();

	if (IsValid(Inventory))
	{
		Inventory->OnInventorySlotChanged.AddUniqueDynamic(this, &UQuestLogComponent::HandleInventorySlotChanged);
	}

	// The Experience Component credits the companion's kills to the player too; without one, only the player's own count.
	if (IsValid(ExperienceComponent))
	{
		ExperienceComponent->OnKillAwarded.AddUniqueDynamic(this, &UQuestLogComponent::HandleKill);
	}
	else if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnKilled.AddUniqueDynamic(this, &UQuestLogComponent::HandleKill);
	}
}

void UQuestLogComponent::UnbindFromPawn()
{
	if (IsValid(Inventory))
	{
		Inventory->OnInventorySlotChanged.RemoveDynamic(this, &UQuestLogComponent::HandleInventorySlotChanged);
	}

	if (IsValid(ExperienceComponent))
	{
		ExperienceComponent->OnKillAwarded.RemoveDynamic(this, &UQuestLogComponent::HandleKill);
	}

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnKilled.RemoveDynamic(this, &UQuestLogComponent::HandleKill);
	}

	Inventory = nullptr;
	ExperienceComponent = nullptr;
	CombatantComponent = nullptr;
}

int32 UQuestLogComponent::GetPlayerLevel() const
{
	return IsValid(ExperienceComponent) ? ExperienceComponent->GetLevel() : 1;
}

/* -------------------- State -------------------- */

int32 UQuestLogComponent::FindActiveQuestIndex(const UQuestDataAsset* Quest) const
{
	for (int32 Index = 0; Index < ActiveQuests.Num(); ++Index)
	{
		if (ActiveQuests[Index].Quest == Quest)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

int32 UQuestLogComponent::GetObjectiveCountInternal(const FActiveQuest& ActiveQuest, const int32 ObjectiveIndex) const
{
	if (!IsValid(ActiveQuest.Quest) || !ActiveQuest.Quest->Objectives.IsValidIndex(ObjectiveIndex))
	{
		return 0;
	}

	const FQuestObjective& Objective = ActiveQuest.Quest->Objectives[ObjectiveIndex];
	int32 Count = 0;

	switch (Objective.Type)
	{
	case EQuestObjectiveType::Collect:
		Count = IsValid(Inventory) && IsValid(Objective.Item) ? Inventory->GetItemCount(Objective.Item) : 0;
		break;

	case EQuestObjectiveType::WorldFlag:
		{
			const UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this);
			Count = IsValid(Progress) && Progress->HasFlag(Objective.Flag) ? 1 : 0;
		}
		break;

	default:
		Count = ActiveQuest.Counts.IsValidIndex(ObjectiveIndex) ? ActiveQuest.Counts[ObjectiveIndex] : 0;
		break;
	}

	return FMath::Clamp(Count, 0, Objective.GetRequiredCount());
}

int32 UQuestLogComponent::GetObjectiveCount(const UQuestDataAsset* Quest, const int32 ObjectiveIndex) const
{
	const int32 QuestIndex = FindActiveQuestIndex(Quest);
	return QuestIndex != INDEX_NONE ? GetObjectiveCountInternal(ActiveQuests[QuestIndex], ObjectiveIndex) : 0;
}

bool UQuestLogComponent::AreObjectivesComplete(const FActiveQuest& ActiveQuest) const
{
	if (!IsValid(ActiveQuest.Quest))
	{
		return false;
	}

	for (int32 Index = 0; Index < ActiveQuest.Quest->Objectives.Num(); ++Index)
	{
		if (GetObjectiveCountInternal(ActiveQuest, Index) < ActiveQuest.Quest->Objectives[Index].GetRequiredCount())
		{
			return false;
		}
	}

	return true;
}

EQuestState UQuestLogComponent::GetQuestState(const UQuestDataAsset* Quest) const
{
	if (!IsValid(Quest))
	{
		return EQuestState::Unavailable;
	}

	if (CompletedQuests.Contains(Quest))
	{
		return EQuestState::Completed;
	}

	const int32 QuestIndex = FindActiveQuestIndex(Quest);

	if (QuestIndex != INDEX_NONE)
	{
		return AreObjectivesComplete(ActiveQuests[QuestIndex]) ? EQuestState::ReadyToTurnIn : EQuestState::Active;
	}

	if (GetPlayerLevel() < Quest->RequiredLevel)
	{
		return EQuestState::Unavailable;
	}

	for (const UQuestDataAsset* Prerequisite : Quest->Prerequisites)
	{
		if (IsValid(Prerequisite) && !CompletedQuests.Contains(Prerequisite))
		{
			return EQuestState::Unavailable;
		}
	}

	const UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this);

	if (IsValid(Progress) && !Progress->HasAllFlags(Quest->RequiredFlags))
	{
		return EQuestState::Unavailable;
	}

	return EQuestState::Available;
}

/* -------------------- Taking and handing in -------------------- */

bool UQuestLogComponent::AcceptQuest(UQuestDataAsset* Quest)
{
	if (GetQuestState(Quest) != EQuestState::Available)
	{
		return false;
	}

	FActiveQuest& ActiveQuest = ActiveQuests.AddDefaulted_GetRef();
	ActiveQuest.Quest = Quest;
	ActiveQuest.Counts.SetNumZeroed(Quest->Objectives.Num());

	if (UAdventureComponent* Adventure = GetAdventure())
	{
		Adventure->Notify(FText::Format(LOCTEXT("Accepted", "Quest accepted: {0}"), Quest->Title));
	}

	OnQuestLogChanged.Broadcast();
	return true;
}

bool UQuestLogComponent::TurnInQuest(UQuestDataAsset* Quest)
{
	if (GetQuestState(Quest) != EQuestState::ReadyToTurnIn)
	{
		return false;
	}

	if (Quest->bConsumeCollectedItems && IsValid(Inventory))
	{
		for (const FQuestObjective& Objective : Quest->Objectives)
		{
			if (Objective.Type == EQuestObjectiveType::Collect && IsValid(Objective.Item))
			{
				Inventory->RemoveItem(Objective.Item, Objective.GetRequiredCount());
			}
		}
	}

	ActiveQuests.RemoveAt(FindActiveQuestIndex(Quest));
	CompletedQuests.Add(Quest);

	UAdventureComponent* Adventure = GetAdventure();

	if (IsValid(Adventure))
	{
		Adventure->Notify(FText::Format(LOCTEXT("Completed", "Quest completed: {0}"), Quest->Title));
		Adventure->GiveExperience(Quest->ExperienceReward);
		Adventure->GiveGold(Quest->GoldReward);

		for (const FInventorySlot& Reward : Quest->ItemRewards)
		{
			if (!Reward.IsEmpty())
			{
				Adventure->GiveItem(Reward.Item, Reward.Count);
			}
		}
	}

	OnQuestLogChanged.Broadcast();

	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->SetFlag(Quest->FlagOnComplete);
		Progress->SaveGame();
	}

	return true;
}

bool UQuestLogComponent::AbandonQuest(UQuestDataAsset* Quest)
{
	const int32 QuestIndex = FindActiveQuestIndex(Quest);

	if (QuestIndex == INDEX_NONE)
	{
		return false;
	}

	ActiveQuests.RemoveAt(QuestIndex);
	OnQuestLogChanged.Broadcast();
	return true;
}

/* -------------------- Counting -------------------- */

void UQuestLogComponent::GetReadyStates(TArray<bool>& OutReady) const
{
	OutReady.Reset(ActiveQuests.Num());

	for (const FActiveQuest& ActiveQuest : ActiveQuests)
	{
		OutReady.Add(AreObjectivesComplete(ActiveQuest));
	}
}

void UQuestLogComponent::AnnounceNewlyReady(const TArray<bool>& WasReady)
{
	UAdventureComponent* Adventure = GetAdventure();

	for (int32 Index = 0; Index < ActiveQuests.Num(); ++Index)
	{
		const bool bWasReady = WasReady.IsValidIndex(Index) && WasReady[Index];

		if (!bWasReady && IsValid(Adventure) && IsValid(ActiveQuests[Index].Quest) && AreObjectivesComplete(ActiveQuests[Index]))
		{
			Adventure->Notify(FText::Format(LOCTEXT("Ready", "{0}: all done. Return to hand it in."), ActiveQuests[Index].Quest->Title));
		}
	}
}

void UQuestLogComponent::HandleKill(AActor* Victim)
{
	if (!IsValid(Victim) || ActiveQuests.IsEmpty())
	{
		return;
	}

	TArray<bool> WasReady;
	GetReadyStates(WasReady);

	UAdventureComponent* Adventure = GetAdventure();
	bool bChanged = false;

	for (FActiveQuest& ActiveQuest : ActiveQuests)
	{
		if (!IsValid(ActiveQuest.Quest))
		{
			continue;
		}

		for (int32 Index = 0; Index < ActiveQuest.Quest->Objectives.Num(); ++Index)
		{
			const FQuestObjective& Objective = ActiveQuest.Quest->Objectives[Index];

			if (Objective.Type != EQuestObjectiveType::Kill || !Objective.TargetClass || !Victim->IsA(Objective.TargetClass)
				|| !ActiveQuest.Counts.IsValidIndex(Index) || ActiveQuest.Counts[Index] >= Objective.GetRequiredCount())
			{
				continue;
			}

			++ActiveQuest.Counts[Index];
			bChanged = true;

			if (IsValid(Adventure))
			{
				Adventure->Notify(FText::Format(LOCTEXT("KillProgress", "{0}: {1}/{2}"),
					Objective.Description, ActiveQuest.Counts[Index], Objective.GetRequiredCount()));
			}
		}
	}

	if (bChanged)
	{
		AnnounceNewlyReady(WasReady);
		OnQuestLogChanged.Broadcast();
	}
}

void UQuestLogComponent::NotifyTalkedTo(const FName NpcId)
{
	if (NpcId.IsNone() || ActiveQuests.IsEmpty())
	{
		return;
	}

	TArray<bool> WasReady;
	GetReadyStates(WasReady);

	bool bChanged = false;

	for (FActiveQuest& ActiveQuest : ActiveQuests)
	{
		if (!IsValid(ActiveQuest.Quest))
		{
			continue;
		}

		for (int32 Index = 0; Index < ActiveQuest.Quest->Objectives.Num(); ++Index)
		{
			const FQuestObjective& Objective = ActiveQuest.Quest->Objectives[Index];

			if (Objective.Type == EQuestObjectiveType::TalkTo && Objective.NpcId == NpcId
				&& ActiveQuest.Counts.IsValidIndex(Index) && ActiveQuest.Counts[Index] < 1)
			{
				ActiveQuest.Counts[Index] = 1;
				bChanged = true;
			}
		}
	}

	if (bChanged)
	{
		AnnounceNewlyReady(WasReady);
		OnQuestLogChanged.Broadcast();
	}
}

void UQuestLogComponent::HandleInventorySlotChanged(int32 SlotIndex)
{
	// Collect objectives are read from the bag whenever they're shown; listeners just need to look again.
	if (!ActiveQuests.IsEmpty())
	{
		OnQuestLogChanged.Broadcast();
	}
}

void UQuestLogComponent::HandleFlagSet(FName Flag)
{
	// A flag can finish a World Flag objective, or make new quests available at a villager.
	OnQuestLogChanged.Broadcast();
}

/* -------------------- Text -------------------- */

FText UQuestLogComponent::BuildObjectivesText(const UQuestDataAsset* Quest) const
{
	if (!IsValid(Quest))
	{
		return FText::GetEmpty();
	}

	const int32 QuestIndex = FindActiveQuestIndex(Quest);
	FString Lines;

	for (int32 Index = 0; Index < Quest->Objectives.Num(); ++Index)
	{
		const FQuestObjective& Objective = Quest->Objectives[Index];
		const int32 Count = QuestIndex != INDEX_NONE ? GetObjectiveCountInternal(ActiveQuests[QuestIndex], Index) : 0;
		const FString Description = Objective.Description.IsEmpty() ? FString(TEXT("Objective")) : Objective.Description.ToString();

		if (!Lines.IsEmpty())
		{
			Lines += TEXT("\n");
		}

		Lines += FString::Printf(TEXT("  - %s: %d/%d"), *Description, Count, Objective.GetRequiredCount());
	}

	return FText::FromString(Lines);
}

FText UQuestLogComponent::BuildTrackerText() const
{
	FString Lines;

	for (const FActiveQuest& ActiveQuest : ActiveQuests)
	{
		if (!IsValid(ActiveQuest.Quest))
		{
			continue;
		}

		if (!Lines.IsEmpty())
		{
			Lines += TEXT("\n\n");
		}

		Lines += ActiveQuest.Quest->Title.ToString();

		if (AreObjectivesComplete(ActiveQuest))
		{
			Lines += TEXT("\n  (ready to hand in)");
		}
		else
		{
			Lines += TEXT("\n");
			Lines += BuildObjectivesText(ActiveQuest.Quest).ToString();
		}
	}

	return FText::FromString(Lines);
}

/* -------------------- Saving -------------------- */

void UQuestLogComponent::WriteSaveData(UAdventureSaveGame& Save) const
{
	Save.Quests.Reset();

	for (const FActiveQuest& ActiveQuest : ActiveQuests)
	{
		if (IsValid(ActiveQuest.Quest))
		{
			FSavedQuest& Saved = Save.Quests.AddDefaulted_GetRef();
			Saved.Quest = FSoftObjectPath(ActiveQuest.Quest.Get());
			Saved.Counts = ActiveQuest.Counts;
		}
	}

	for (const UQuestDataAsset* Quest : CompletedQuests)
	{
		if (IsValid(Quest))
		{
			FSavedQuest& Saved = Save.Quests.AddDefaulted_GetRef();
			Saved.Quest = FSoftObjectPath(Quest);
			Saved.bCompleted = true;
		}
	}
}

void UQuestLogComponent::ApplySaveData(const UAdventureSaveGame& Save)
{
	ActiveQuests.Reset();
	CompletedQuests.Reset();

	for (const FSavedQuest& Saved : Save.Quests)
	{
		UQuestDataAsset* Quest = Cast<UQuestDataAsset>(Saved.Quest.TryLoad());

		// A quest asset deleted or renamed since the save is simply forgotten.
		if (!IsValid(Quest))
		{
			continue;
		}

		if (Saved.bCompleted)
		{
			CompletedQuests.AddUnique(Quest);
			continue;
		}

		FActiveQuest& ActiveQuest = ActiveQuests.AddDefaulted_GetRef();
		ActiveQuest.Quest = Quest;
		ActiveQuest.Counts = Saved.Counts;
		ActiveQuest.Counts.SetNumZeroed(Quest->Objectives.Num());
	}

	OnQuestLogChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
