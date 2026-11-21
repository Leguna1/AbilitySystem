#include "TreasureChest.h"

#include "AdventureComponent.h"
#include "ItemDataAsset.h"
#include "LootDropComponent.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "TreasureChest"

ATreasureChest::ATreasureChest()
{
	SetPlaceholder(EAdventurePropShape::Cube, FVector(1.0f, 0.7f, 0.6f), 120.0f);
	DisplayName = LOCTEXT("DefaultName", "Chest");

	LootDropComponent = CreateDefaultSubobject<ULootDropComponent>(TEXT("LootDropComponent"));
	LootDropComponent->SetDropOnDeath(false);
}

void ATreasureChest::BeginPlay()
{
	Super::BeginPlay();

	const UWorldProgressSubsystem* Progress = GetProgress();

	if (IsValid(Progress) && Progress->HasFlag(ChestId))
	{
		ApplyOpenedState(true);
	}
	else
	{
		SetStateColor(FLinearColor(1.0f, 0.75f, 0.2f));
	}
}

bool ATreasureChest::IsSealed() const
{
	const UWorldProgressSubsystem* Progress = GetProgress();
	return IsValid(Progress) && !Progress->HasAllFlags(RequiredFlags);
}

bool ATreasureChest::HasKey(const AActor* Interactor) const
{
	if (!IsValid(KeyItem))
	{
		return true;
	}

	const UInventoryComponent* Inventory = IsValid(Interactor) ? Interactor->FindComponentByClass<UInventoryComponent>() : nullptr;
	return IsValid(Inventory) && Inventory->GetItemCount(KeyItem) > 0;
}

FText ATreasureChest::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	if (bOpened)
	{
		return FText::GetEmpty();
	}

	if (IsSealed())
	{
		return FText::Format(LOCTEXT("Sealed", "{0} is sealed"), DisplayName);
	}

	if (!HasKey(Interactor))
	{
		return FText::Format(LOCTEXT("NeedsKey", "Locked. Needs {0}"), KeyItem->GetDisplayName());
	}

	return FText::Format(LOCTEXT("Open", "Open {0}"), DisplayName);
}

bool ATreasureChest::CanInteract_Implementation(AActor* Interactor) const
{
	return !bOpened && !IsSealed() && HasKey(Interactor);
}

void ATreasureChest::Interact_Implementation(AActor* Interactor)
{
	UAdventureComponent* Adventure = GetAdventure();

	if (!IsValid(Adventure) || !CanInteract_Implementation(Interactor))
	{
		return;
	}

	if (IsValid(KeyItem) && bConsumeKey)
	{
		if (UInventoryComponent* Inventory = Interactor->FindComponentByClass<UInventoryComponent>())
		{
			Inventory->RemoveItem(KeyItem, 1);
		}
	}

	for (const FInventorySlot& Stack : Contents)
	{
		if (!Stack.IsEmpty())
		{
			Adventure->GiveItem(Stack.Item, Stack.Count);
		}
	}

	if (GoldMax > 0 || GoldMin > 0)
	{
		Adventure->GiveGold(FMath::RandRange(FMath::Min(GoldMin, GoldMax), FMath::Max(GoldMin, GoldMax)));
	}

	for (const FInventorySlot& Rolled : LootDropComponent->RollLoot())
	{
		if (!Rolled.IsEmpty())
		{
			Adventure->GiveItem(Rolled.Item, Rolled.Count);
		}
	}

	ApplyOpenedState(false);

	if (UWorldProgressSubsystem* Progress = GetProgress())
	{
		Progress->SetFlag(ChestId);
		Progress->SaveGame();
	}
}

void ATreasureChest::ApplyOpenedState(const bool bFromSave)
{
	bOpened = true;
	SetGlowEnabled(false);
	SetStateColor(FLinearColor(0.35f, 0.3f, 0.25f));
	SetLabelStatus(LOCTEXT("Empty", "Empty"));
	OnChestOpened(bFromSave);
}

#undef LOCTEXT_NAMESPACE
