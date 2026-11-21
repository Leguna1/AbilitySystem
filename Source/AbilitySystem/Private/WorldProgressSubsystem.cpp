#include "WorldProgressSubsystem.h"

#include "AbilitySystemLog.h"
#include "AdventureComponent.h"
#include "AdventureSaveGame.h"
#include "AdventureSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Package.h"

const FName UWorldProgressSubsystem::VictoryFlag(TEXT("Game.Victory"));

namespace
{
	void ResetAdventureSave()
	{
		const UAdventureSettings* Settings = UAdventureSettings::Get();

		if (UGameplayStatics::DoesSaveGameExist(Settings->SaveSlotName, 0))
		{
			UGameplayStatics::DeleteGameInSlot(Settings->SaveSlotName, 0);
			UE_LOG(LogAbilitySystem, Display, TEXT("Adventure save '%s' deleted. It takes effect the next time the level starts."), *Settings->SaveSlotName);
		}
		else
		{
			UE_LOG(LogAbilitySystem, Display, TEXT("No adventure save '%s' to delete."), *Settings->SaveSlotName);
		}
	}

	FAutoConsoleCommand ResetAdventureSaveCommand(
		TEXT("Adventure.ResetSave"),
		TEXT("Deletes the adventure save (flags, quests, level, bag). Restart the level afterwards."),
		FConsoleCommandDelegate::CreateStatic(&ResetAdventureSave));
}

UWorldProgressSubsystem* UWorldProgressSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return IsValid(World) ? World->GetSubsystem<UWorldProgressSubsystem>() : nullptr;
}

bool UWorldProgressSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWorldProgressSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UAdventureSettings* Settings = UAdventureSettings::Get();

	// Before any actor's Begin Play, so a taken altar or a dead boss can set itself up right away.
	if (Settings->bEnableSaving && UGameplayStatics::DoesSaveGameExist(Settings->SaveSlotName, 0))
	{
		LoadedSave = Cast<UAdventureSaveGame>(UGameplayStatics::LoadGameFromSlot(Settings->SaveSlotName, 0));

		if (IsValid(LoadedSave))
		{
			WorldFlags = LoadedSave->Flags;
			UE_LOG(LogAbilitySystem, Display, TEXT("Adventure save loaded: %d flags. Console command Adventure.ResetSave starts over."), WorldFlags.Num());
		}
	}
}

/* -------------------- Flags -------------------- */

void UWorldProgressSubsystem::SetFlag(const FName Flag)
{
	if (Flag.IsNone() || WorldFlags.Contains(Flag))
	{
		return;
	}

	WorldFlags.Add(Flag);
	OnFlagSet.Broadcast(Flag);

	if (Flag != VictoryFlag)
	{
		CheckVictory();
	}
}

bool UWorldProgressSubsystem::HasFlag(const FName Flag) const
{
	return !Flag.IsNone() && WorldFlags.Contains(Flag);
}

bool UWorldProgressSubsystem::HasAllFlags(const TArray<FName>& InFlags) const
{
	for (const FName& Flag : InFlags)
	{
		if (!Flag.IsNone() && !WorldFlags.Contains(Flag))
		{
			return false;
		}
	}

	return true;
}

/* -------------------- Objectives and victory -------------------- */

void UWorldProgressSubsystem::RegisterObjective(const FName Flag, const FText DisplayName, const bool bBoss)
{
	if (Flag.IsNone())
	{
		return;
	}

	for (const FWorldObjective& Existing : Objectives)
	{
		if (Existing.Flag == Flag)
		{
			return;
		}
	}

	FWorldObjective& Objective = Objectives.AddDefaulted_GetRef();
	Objective.Flag = Flag;
	Objective.DisplayName = DisplayName;
	Objective.bBoss = bBoss;
}

int32 UWorldProgressSubsystem::GetCompletedObjectiveCount() const
{
	int32 Completed = 0;

	for (const FWorldObjective& Objective : Objectives)
	{
		Completed += WorldFlags.Contains(Objective.Flag) ? 1 : 0;
	}

	return Completed;
}

void UWorldProgressSubsystem::CheckVictory()
{
	if (Objectives.IsEmpty() || IsVictoryAchieved() || GetCompletedObjectiveCount() < Objectives.Num())
	{
		return;
	}

	SetFlag(VictoryFlag);
	OnVictory.Broadcast();
	SaveGame();
}

/* -------------------- Interactables -------------------- */

void UWorldProgressSubsystem::RegisterInteractable(AActor* Actor)
{
	if (IsValid(Actor))
	{
		Interactables.AddUnique(Actor);
	}
}

void UWorldProgressSubsystem::UnregisterInteractable(AActor* Actor)
{
	Interactables.Remove(Actor);
}

/* -------------------- Player -------------------- */

void UWorldProgressSubsystem::NotifyPlayerReady(APawn* PlayerPawn)
{
	ReadyPlayerPawn = PlayerPawn;

	if (IsValid(PlayerPawn))
	{
		OnPlayerReady.Broadcast(PlayerPawn);
	}
}

/* -------------------- Saving -------------------- */

void UWorldProgressSubsystem::SaveGame()
{
	const UAdventureSettings* Settings = UAdventureSettings::Get();

	if (!Settings->bEnableSaving)
	{
		return;
	}

	UAdventureSaveGame* Save = Cast<UAdventureSaveGame>(UGameplayStatics::CreateSaveGameObject(UAdventureSaveGame::StaticClass()));

	if (!IsValid(Save))
	{
		return;
	}

	Save->Flags = WorldFlags;

	const UAdventureComponent* Adventure = UAdventureComponent::Get(this);

	if (IsValid(Adventure) && Adventure->IsPlayerReady())
	{
		Adventure->WriteSaveData(*Save);
	}
	else if (IsValid(LoadedSave) && LoadedSave->bHasPlayerData)
	{
		// No player to ask yet: keep what the last save knew about them instead of wiping it.
		const TArray<FName> CurrentFlags = Save->Flags;
		Save = DuplicateObject<UAdventureSaveGame>(LoadedSave.Get(), GetTransientPackage());
		Save->Flags = CurrentFlags;
	}

	if (UGameplayStatics::SaveGameToSlot(Save, Settings->SaveSlotName, 0))
	{
		LoadedSave = Save;
	}
	else
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("Adventure save '%s' could not be written."), *Settings->SaveSlotName);
	}
}
