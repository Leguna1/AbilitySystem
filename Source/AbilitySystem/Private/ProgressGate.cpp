#include "ProgressGate.h"

#include "AdventureComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "InventoryComponent.h"
#include "ItemDataAsset.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "ProgressGate"

AProgressGate::AProgressGate()
{
	// A wall segment: 4 m wide, 4 m tall, half a meter thick. Scale the actor to fit the passage.
	SetPlaceholder(EAdventurePropShape::Cube, FVector(0.5f, 4.0f, 4.0f), 430.0f);
	DisplayName = LOCTEXT("DefaultName", "Sealed Gate");
	SealedHint = LOCTEXT("DefaultHint", "Sealed. Something elsewhere must be done first.");
}

void AProgressGate::BeginPlay()
{
	Super::BeginPlay();

	UWorldProgressSubsystem* Progress = GetProgress();

	if (IsValid(Progress))
	{
		Progress->OnFlagSet.AddUniqueDynamic(this, &AProgressGate::HandleFlagSet);
	}

	const bool bOpenedBefore = IsValid(Progress) && Progress->HasFlag(GateId);
	const bool bFlagsMet = IsValid(Progress) && !RequiredFlags.IsEmpty() && Progress->HasAllFlags(RequiredFlags);

	bOpen = bStartsOpen || bOpenedBefore || bFlagsMet;
	ApplyState();
}

void AProgressGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorldProgressSubsystem* Progress = GetProgress())
	{
		Progress->OnFlagSet.RemoveDynamic(this, &AProgressGate::HandleFlagSet);
	}

	Super::EndPlay(EndPlayReason);
}

void AProgressGate::HandleFlagSet(FName Flag)
{
	const UWorldProgressSubsystem* Progress = GetProgress();

	if (!bOpen && IsValid(Progress) && !RequiredFlags.IsEmpty() && RequiredFlags.Contains(Flag) && Progress->HasAllFlags(RequiredFlags))
	{
		OpenGate(true);
	}
}

void AProgressGate::OpenGate(const bool bAnnounce)
{
	if (bOpen)
	{
		return;
	}

	bOpen = true;
	ApplyState();

	if (UWorldProgressSubsystem* Progress = GetProgress())
	{
		Progress->SetFlag(GateId);
	}

	if (bAnnounce)
	{
		if (UAdventureComponent* Adventure = GetAdventure())
		{
			Adventure->Notify(FText::Format(LOCTEXT("Opened", "{0} opens."), DisplayName));
		}
	}
}

void AProgressGate::CloseGate()
{
	if (!bOpen)
	{
		return;
	}

	bOpen = false;
	ApplyState();
}

void AProgressGate::ApplyState()
{
	if (IsValid(Mesh))
	{
		Mesh->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
		Mesh->SetVisibility(!bOpen);
	}

	SetGlowEnabled(!bOpen);
	SetStateColor(SealedColor);
	SetLabelStatus(FText::GetEmpty());

	if (IsValid(Label))
	{
		Label->SetVisibility(bShowLabel && !bOpen);
	}

	OnGateStateChanged(bOpen);
}

bool AProgressGate::HasKey(const AActor* Interactor) const
{
	const UInventoryComponent* Inventory = IsValid(Interactor) ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	return IsValid(KeyItem) && IsValid(Inventory) && Inventory->GetItemCount(KeyItem) > 0;
}

FText AProgressGate::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	if (bOpen)
	{
		return FText::GetEmpty();
	}

	if (!IsValid(KeyItem))
	{
		return SealedHint;
	}

	return HasKey(Interactor)
		? FText::Format(LOCTEXT("Unlock", "Unlock with {0}"), KeyItem->GetDisplayName())
		: FText::Format(LOCTEXT("NeedsKey", "Locked. Needs {0}"), KeyItem->GetDisplayName());
}

bool AProgressGate::CanInteract_Implementation(AActor* Interactor) const
{
	return !bOpen && HasKey(Interactor);
}

void AProgressGate::Interact_Implementation(AActor* Interactor)
{
	if (!CanInteract_Implementation(Interactor))
	{
		return;
	}

	if (bConsumeKey)
	{
		if (UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>())
		{
			Inventory->RemoveItem(KeyItem, 1);
		}
	}

	OpenGate(true);

	if (UWorldProgressSubsystem* Progress = GetProgress())
	{
		Progress->SaveGame();
	}
}

#undef LOCTEXT_NAMESPACE
