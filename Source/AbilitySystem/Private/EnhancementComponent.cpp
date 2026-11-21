#include "EnhancementComponent.h"

#include "AdventureSaveGame.h"
#include "AdventureSettings.h"
#include "CombatantComponent.h"
#include "CurrencyDataAsset.h"
#include "EquipmentDataAsset.h"
#include "InventoryComponent.h"
#include "ItemDataAsset.h"

UEnhancementComponent::UEnhancementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnhancementComponent::BeginPlay()
{
	Super::BeginPlay();

	Inventory = GetOwner()->FindComponentByClass<UInventoryComponent>();
	CombatantComponent = GetOwner()->FindComponentByClass<UCombatantComponent>();

	if (IsValid(Inventory))
	{
		Inventory->OnEquipmentChanged.AddUniqueDynamic(this, &UEnhancementComponent::HandleEquipmentChanged);
	}
}

void UEnhancementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Inventory))
	{
		Inventory->OnEquipmentChanged.RemoveDynamic(this, &UEnhancementComponent::HandleEquipmentChanged);
	}

	if (IsValid(CombatantComponent))
	{
		for (const UEnhancementSlotSource* Source : SlotSources)
		{
			CombatantComponent->RemoveEffectsFromSource(Source);
		}
	}

	Super::EndPlay(EndPlayReason);
}

/* -------------------- Slots -------------------- */

int32 UEnhancementComponent::FindSlotState(const FGameplayTag SlotTag) const
{
	for (int32 Index = 0; Index < SlotStates.Num(); ++Index)
	{
		if (SlotStates[Index].SlotTag == SlotTag)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

UEnhancementComponent::FSlotState& UEnhancementComponent::FindOrAddSlotState(const FGameplayTag SlotTag)
{
	const int32 Existing = FindSlotState(SlotTag);

	if (Existing != INDEX_NONE)
	{
		return SlotStates[Existing];
	}

	UEnhancementSlotSource* Source = NewObject<UEnhancementSlotSource>(this);
	SlotSources.Add(Source);

	FSlotState& State = SlotStates.AddDefaulted_GetRef();
	State.SlotTag = SlotTag;
	State.Source = Source;
	return State;
}

int32 UEnhancementComponent::GetEnhancementLevel(const FGameplayTag SlotTag) const
{
	const int32 Index = FindSlotState(SlotTag);
	return Index != INDEX_NONE ? SlotStates[Index].Level : 0;
}

void UEnhancementComponent::SetEnhancementLevel(const FGameplayTag SlotTag, const int32 NewLevel)
{
	if (!SlotTag.IsValid())
	{
		return;
	}

	const int32 Clamped = FMath::Clamp(NewLevel, 0, UAdventureSettings::Get()->MaxEnhancementLevel);
	FSlotState& State = FindOrAddSlotState(SlotTag);

	if (State.Level == Clamped)
	{
		return;
	}

	State.Level = Clamped;
	RefreshSlotBonus(SlotTag);
	OnEnhancementChanged.Broadcast(SlotTag, Clamped);
}

void UEnhancementComponent::RefreshSlotBonus(const FGameplayTag SlotTag)
{
	const int32 Index = FindSlotState(SlotTag);

	if (Index == INDEX_NONE || !IsValid(CombatantComponent))
	{
		return;
	}

	const UEnhancementSlotSource* Source = SlotStates[Index].Source.Get();
	const int32 Level = SlotStates[Index].Level;

	if (!IsValid(Source))
	{
		return;
	}

	CombatantComponent->RemoveEffectsFromSource(Source);

	const UEquipmentDataAsset* Item = IsValid(Inventory) ? Inventory->GetEquippedItemInSlot(SlotTag) : nullptr;

	if (Level <= 0 || !IsValid(Item) || Item->GetStatModifiers().IsEmpty())
	{
		return;
	}

	// The item's own modifiers again, scaled: the slot's level multiplies what the item gives.
	const float Scale = static_cast<float>(Level) * UAdventureSettings::Get()->EnhancementBonusPerLevel;
	TArray<FStatModifier> Bonus;

	for (const FStatModifier& ItemModifier : Item->GetStatModifiers())
	{
		FStatModifier& Scaled = Bonus.Add_GetRef(ItemModifier);
		Scaled.Magnitude = FAbilityRankedFloat(ItemModifier.Magnitude.Get(1) * Scale);
	}

	// Unkeyed, like the equipment's own stats: permanent while worn, so it stays off the active effects bar.
	CombatantComponent->ApplyEffect(Source, NAME_None, Bonus, 1, FGameplayTagContainer(), 0.0f);
}

void UEnhancementComponent::HandleEquipmentChanged(int32 EquipmentSlotIndex, UEquipmentDataAsset* PreviousItem, UEquipmentDataAsset* NewItem)
{
	if (IsValid(Inventory))
	{
		RefreshSlotBonus(Inventory->GetEquipmentSlotDefinition(EquipmentSlotIndex).SlotTag);
	}
}

/* -------------------- Enhancing -------------------- */

int32 UEnhancementComponent::GetNextLevelCost(const FGameplayTag SlotTag) const
{
	const UAdventureSettings* Settings = UAdventureSettings::Get();
	const int32 NextLevel = GetEnhancementLevel(SlotTag) + 1;

	if (NextLevel > Settings->MaxEnhancementLevel)
	{
		return 0;
	}

	const float Cost = static_cast<float>(Settings->EnhancementBaseCost) * FMath::Pow(static_cast<float>(NextLevel), Settings->EnhancementCostGrowth);
	return FMath::Max(FMath::RoundToInt(Cost), 0);
}

float UEnhancementComponent::GetNextLevelChance(const FGameplayTag SlotTag) const
{
	const TArray<float>& Chances = UAdventureSettings::Get()->EnhancementSuccessChance;

	if (Chances.IsEmpty())
	{
		return 100.0f;
	}

	// Entry 0 is the chance to reach +1.
	const int32 ChanceIndex = FMath::Clamp(GetEnhancementLevel(SlotTag), 0, Chances.Num() - 1);
	return FMath::Clamp(Chances[ChanceIndex], 0.0f, 100.0f);
}

bool UEnhancementComponent::CanEnhance(const FGameplayTag SlotTag) const
{
	const UAdventureSettings* Settings = UAdventureSettings::Get();

	if (!IsValid(Inventory) || !IsValid(Inventory->GetEquippedItemInSlot(SlotTag))
		|| GetEnhancementLevel(SlotTag) >= Settings->MaxEnhancementLevel)
	{
		return false;
	}

	const int32 Cost = GetNextLevelCost(SlotTag);
	const UCurrencyDataAsset* Gold = Settings->GoldCurrency.LoadSynchronous();

	if (Cost > 0 && IsValid(Gold) && !Inventory->CanAfford(Gold, Cost))
	{
		return false;
	}

	const UItemDataAsset* Material = Settings->EnhancementMaterial.LoadSynchronous();
	return !IsValid(Material) || Inventory->GetItemCount(Material) >= Settings->EnhancementMaterialsPerAttempt;
}

EEnhanceResult UEnhancementComponent::TryEnhance(const FGameplayTag SlotTag)
{
	if (!CanEnhance(SlotTag))
	{
		return EEnhanceResult::NotPossible;
	}

	const UAdventureSettings* Settings = UAdventureSettings::Get();
	const int32 Cost = GetNextLevelCost(SlotTag);
	const float Chance = GetNextLevelChance(SlotTag);

	if (UCurrencyDataAsset* Gold = Settings->GoldCurrency.LoadSynchronous())
	{
		Inventory->SpendCurrency(Gold, Cost);
	}

	if (const UItemDataAsset* Material = Settings->EnhancementMaterial.LoadSynchronous())
	{
		Inventory->RemoveItem(Material, Settings->EnhancementMaterialsPerAttempt);
	}

	const int32 CurrentLevel = GetEnhancementLevel(SlotTag);

	if (FMath::FRandRange(0.0f, 100.0f) < Chance)
	{
		SetEnhancementLevel(SlotTag, CurrentLevel + 1);
		return EEnhanceResult::Success;
	}

	if (Settings->bDowngradeOnFailure && CurrentLevel >= Settings->DowngradeFromLevel)
	{
		SetEnhancementLevel(SlotTag, CurrentLevel - 1);
	}

	return EEnhanceResult::Failed;
}

/* -------------------- Saving -------------------- */

void UEnhancementComponent::WriteSaveData(UAdventureSaveGame& Save) const
{
	Save.EnhancementLevels.Reset();

	for (const FSlotState& State : SlotStates)
	{
		if (State.Level > 0)
		{
			FSavedTagValue& Saved = Save.EnhancementLevels.AddDefaulted_GetRef();
			Saved.Tag = State.SlotTag;
			Saved.Value = State.Level;
		}
	}
}

void UEnhancementComponent::ApplySaveData(const UAdventureSaveGame& Save)
{
	for (const FSavedTagValue& Saved : Save.EnhancementLevels)
	{
		SetEnhancementLevel(Saved.Tag, Saved.Value);
	}
}
