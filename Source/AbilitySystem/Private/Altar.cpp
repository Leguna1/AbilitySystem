#include "Altar.h"

#include "AdventureComponent.h"
#include "CombatantComponent.h"
#include "EnemySpawner.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "Altar"

AAltar::AAltar()
{
	SetPlaceholder(EAdventurePropShape::Cylinder, FVector(1.6f, 1.6f, 1.2f), 230.0f);
	DisplayName = LOCTEXT("DefaultName", "Altar");
}

void AAltar::BeginPlay()
{
	Super::BeginPlay();

	UWorldProgressSubsystem* Progress = GetProgress();

	if (IsValid(Progress))
	{
		Progress->RegisterObjective(AltarId, DisplayName, false);
	}

	// The save is loaded before any actor begins play, so a cleansed altar starts out cleansed.
	if (IsCleansed())
	{
		ApplyCleansedState(true);
	}
	else
	{
		SetStateColor(CorruptedColor);
		SetLabelStatus(LOCTEXT("Corrupted", "Corrupted"));
	}
}

bool AAltar::IsCleansed() const
{
	const UWorldProgressSubsystem* Progress = GetProgress();
	return IsValid(Progress) && Progress->HasFlag(AltarId);
}

int32 AAltar::GetLivingGuardCount() const
{
	int32 Living = 0;

	for (const AEnemySpawner* Spawner : GuardSpawners)
	{
		Living += IsValid(Spawner) ? Spawner->GetAliveCount() : 0;
	}

	for (const AActor* Guard : PlacedGuards)
	{
		if (!IsValid(Guard))
		{
			continue;
		}

		const UCombatantComponent* Combatant = Guard->FindComponentByClass<UCombatantComponent>();
		Living += IsValid(Combatant) && !Combatant->IsDead() ? 1 : 0;
	}

	return Living;
}

FText AAltar::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	if (IsCleansed())
	{
		return FText::GetEmpty();
	}

	const int32 Guards = GetLivingGuardCount();

	return Guards > 0
		? FText::Format(LOCTEXT("Guarded", "{0} is guarded ({1} left)"), DisplayName, Guards)
		: FText::Format(LOCTEXT("Cleanse", "Cleanse {0}"), DisplayName);
}

bool AAltar::CanInteract_Implementation(AActor* Interactor) const
{
	return !IsCleansed() && GetLivingGuardCount() == 0;
}

void AAltar::Interact_Implementation(AActor* Interactor)
{
	UAdventureComponent* Adventure = GetAdventure();

	if (!IsValid(Adventure) || !CanInteract_Implementation(Interactor))
	{
		return;
	}

	Adventure->BeginChannel(FText::Format(LOCTEXT("Cleansing", "Cleansing {0}"), DisplayName), CaptureTime,
		FSimpleDelegate::CreateUObject(this, &AAltar::CompleteCapture));
}

void AAltar::CompleteCapture()
{
	UWorldProgressSubsystem* Progress = GetProgress();

	// A guard that respawned or wandered back during the channel doesn't undo it: the channel was the test.
	if (!IsValid(Progress) || IsCleansed())
	{
		return;
	}

	// Looks and rewards first, so "altar cleansed" reads before anything the flag sets off (a gate opening, victory).
	if (UAdventureComponent* Adventure = GetAdventure())
	{
		Adventure->ShowBanner(FText::Format(LOCTEXT("CleansedBanner", "{0} cleansed"), DisplayName),
			LOCTEXT("CleansedSubtitle", "Its corruption fades. Somewhere, a seal breaks."), 5.0f);
		Adventure->GiveExperience(ExperienceReward);
	}

	Progress->SetFlag(AltarId);
	ApplyCleansedState(false);
	Progress->SaveGame();
}

void AAltar::ApplyCleansedState(const bool bFromSave)
{
	SetStateColor(CleansedColor);
	SetLabelStatus(LOCTEXT("Cleansed", "Cleansed"));

	// The guards' hold is broken: the ones alive stay until killed, none come back.
	for (AEnemySpawner* Spawner : GuardSpawners)
	{
		if (IsValid(Spawner))
		{
			Spawner->SetRespawnEnabled(false);
		}
	}

	OnCleansed(bFromSave);
}

#undef LOCTEXT_NAMESPACE
