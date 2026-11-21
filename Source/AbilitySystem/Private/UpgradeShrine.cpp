#include "UpgradeShrine.h"

#include "AdventureComponent.h"
#include "CombatantComponent.h"
#include "GameFramework/Pawn.h"
#include "ResourceComponent.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "UpgradeShrine"

AUpgradeShrine::AUpgradeShrine()
{
	SetPlaceholder(EAdventurePropShape::Sphere, FVector(0.7f), 140.0f);
	DisplayName = LOCTEXT("DefaultName", "Shrine of Vigor");
	RewardText = LOCTEXT("DefaultReward", "You feel stronger.");
}

void AUpgradeShrine::BeginPlay()
{
	Super::BeginPlay();

	UWorldProgressSubsystem* Progress = GetProgress();

	if (IsValid(Progress))
	{
		// A claimed shrine gives its bonus again whenever a player pawn is ready (a loaded game, a new pawn).
		Progress->OnPlayerReady.AddUniqueDynamic(this, &AUpgradeShrine::HandlePlayerReady);
	}

	if (IsClaimed())
	{
		ApplyClaimedLook();

		if (IsValid(Progress) && IsValid(Progress->GetReadyPlayerPawn()))
		{
			ApplyBonus(Progress->GetReadyPlayerPawn());
		}
	}
	else
	{
		SetStateColor(FLinearColor(0.3f, 1.0f, 0.4f));
	}
}

void AUpgradeShrine::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorldProgressSubsystem* Progress = GetProgress())
	{
		Progress->OnPlayerReady.RemoveDynamic(this, &AUpgradeShrine::HandlePlayerReady);
	}

	Super::EndPlay(EndPlayReason);
}

bool AUpgradeShrine::IsClaimed() const
{
	const UWorldProgressSubsystem* Progress = GetProgress();
	return IsValid(Progress) && Progress->HasFlag(ShrineId);
}

void AUpgradeShrine::HandlePlayerReady(APawn* PlayerPawn)
{
	if (IsClaimed())
	{
		ApplyBonus(PlayerPawn);
	}
}

void AUpgradeShrine::ApplyBonus(AActor* Player) const
{
	UCombatantComponent* Combatant = IsValid(Player) ? Player->FindComponentByClass<UCombatantComponent>() : nullptr;

	if (!IsValid(Combatant) || Modifiers.IsEmpty())
	{
		return;
	}

	// This shrine is the source, unkeyed: permanent, off the active effects bar, and applying it twice replaces itself.
	Combatant->ApplyEffect(this, NAME_None, Modifiers, 1, FGameplayTagContainer(), 0.0f);
}

void AUpgradeShrine::ApplyClaimedLook()
{
	SetGlowEnabled(false);
	SetStateColor(FLinearColor(0.35f, 0.35f, 0.35f));
	SetLabelStatus(LOCTEXT("Spent", "Spent"));
}

FText AUpgradeShrine::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return IsClaimed() ? FText::GetEmpty() : FText::Format(LOCTEXT("Pray", "Pray at {0}"), DisplayName);
}

void AUpgradeShrine::Interact_Implementation(AActor* Interactor)
{
	UWorldProgressSubsystem* Progress = GetProgress();

	if (!IsValid(Progress) || IsClaimed())
	{
		return;
	}

	Progress->SetFlag(ShrineId);
	ApplyBonus(Interactor);
	ApplyClaimedLook();
	OnClaimed();

	if (UAdventureComponent* Adventure = GetAdventure())
	{
		Adventure->ShowBanner(DisplayName, RewardText, 4.0f);
		Adventure->RestorePlayer();
	}

	Progress->SaveGame();
}

#undef LOCTEXT_NAMESPACE
