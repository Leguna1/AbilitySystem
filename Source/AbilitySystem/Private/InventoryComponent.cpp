#include "InventoryComponent.h"

#include "AbilityComponent.h"
#include "AbilitySystemLog.h"
#include "CombatantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ConsumableDataAsset.h"
#include "EquipmentDataAsset.h"
#include "ResourceComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "ItemDataAsset.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "InventoryComponent"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	CombatantComponent = GetOwner()->FindComponentByClass<UCombatantComponent>();
	AbilityComponent = GetOwner()->FindComponentByClass<UAbilityComponent>();
	ResourceComponent = GetOwner()->FindComponentByClass<UResourceComponent>();

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityCooldownStartedEvent.AddUniqueDynamic(this, &UInventoryComponent::HandleCooldownStarted);
	}

	Slots.SetNum(NumSlots);
	Equipped.Init(nullptr, EquipmentSlots.Num());

	for (const FInventorySlot& Starting : StartingItems)
	{
		if (!Starting.IsEmpty() && AddItem(Starting.Item, Starting.Count) > 0)
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("%s: bag too small for starting item %s."), *GetNameSafe(GetOwner()), *GetNameSafe(Starting.Item));
		}
	}

	for (UEquipmentDataAsset* Item : StartingEquipment)
	{
		const int32 EquipmentSlotIndex = IsValid(Item) ? FindEquipmentSlotIndex(Item->GetEquipmentSlot()) : INDEX_NONE;

		if (EquipmentSlotIndex == INDEX_NONE)
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("%s: starting equipment %s has no matching equipment slot."), *GetNameSafe(GetOwner()), *GetNameSafe(Item));
			continue;
		}

		// Two starting items for one slot: the earlier one goes to the bag rather than vanishing.
		if (UEquipmentDataAsset* Previous = SetEquipped(EquipmentSlotIndex, Item))
		{
			AddItem(Previous);
		}
	}

	BroadcastSlotChanged(INDEX_NONE);
}

void UInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const UEquipmentDataAsset* Item : Equipped)
	{
		RemoveStats(Item);
	}

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityCooldownStartedEvent.RemoveDynamic(this, &UInventoryComponent::HandleCooldownStarted);
	}

	Super::EndPlay(EndPlayReason);
}

/* -------------------- Bag -------------------- */

FInventorySlot UInventoryComponent::GetSlot(const int32 SlotIndex) const
{
	return IsValidSlotIndex(SlotIndex) ? Slots[SlotIndex] : FInventorySlot(nullptr, 0);
}

bool UInventoryComponent::IsSlotEmpty(const int32 SlotIndex) const
{
	return !IsValidSlotIndex(SlotIndex) || Slots[SlotIndex].IsEmpty();
}

int32 UInventoryComponent::GetItemCount(const UItemDataAsset* Item) const
{
	int32 Total = 0;

	for (const FInventorySlot& InventorySlot : Slots)
	{
		if (!InventorySlot.IsEmpty() && InventorySlot.Item == Item)
		{
			Total += InventorySlot.Count;
		}
	}

	return Total;
}

int32 UInventoryComponent::FindFirstEmptySlot() const
{
	return Slots.IndexOfByPredicate([](const FInventorySlot& InventorySlot) { return InventorySlot.IsEmpty(); });
}

int32 UInventoryComponent::AddItem(UItemDataAsset* Item, const int32 Count)
{
	if (!IsValid(Item) || Count <= 0)
	{
		return FMath::Max(Count, 0);
	}

	const int32 MaxStack = Item->GetMaxStackSize();
	int32 Remaining = Count;

	// Existing stacks first, so potions don't spread over the bag.
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num() && Remaining > 0; ++SlotIndex)
	{
		FInventorySlot& InventorySlot = Slots[SlotIndex];

		if (!InventorySlot.IsEmpty() && InventorySlot.Item == Item && InventorySlot.Count < MaxStack)
		{
			const int32 Added = FMath::Min(MaxStack - InventorySlot.Count, Remaining);
			InventorySlot.Count += Added;
			Remaining -= Added;
			BroadcastSlotChanged(SlotIndex);
		}
	}

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num() && Remaining > 0; ++SlotIndex)
	{
		FInventorySlot& InventorySlot = Slots[SlotIndex];

		if (InventorySlot.IsEmpty())
		{
			const int32 Added = FMath::Min(MaxStack, Remaining);
			InventorySlot.Item = Item;
			InventorySlot.Count = Added;
			Remaining -= Added;
			BroadcastSlotChanged(SlotIndex);
		}
	}

	return Remaining;
}

int32 UInventoryComponent::RemoveItemAt(const int32 SlotIndex, const int32 Count)
{
	if (IsSlotEmpty(SlotIndex) || Count <= 0)
	{
		return 0;
	}

	FInventorySlot& InventorySlot = Slots[SlotIndex];
	const int32 Removed = FMath::Min(Count, InventorySlot.Count);

	InventorySlot.Count -= Removed;

	if (InventorySlot.Count <= 0)
	{
		InventorySlot = FInventorySlot(nullptr, 0);
	}

	BroadcastSlotChanged(SlotIndex);
	return Removed;
}

int32 UInventoryComponent::RemoveItem(const UItemDataAsset* Item, const int32 Count)
{
	int32 Removed = 0;

	// From the back, so the stacks the player sees first stay full.
	for (int32 SlotIndex = Slots.Num() - 1; SlotIndex >= 0 && Removed < Count; --SlotIndex)
	{
		if (!Slots[SlotIndex].IsEmpty() && Slots[SlotIndex].Item == Item)
		{
			Removed += RemoveItemAt(SlotIndex, Count - Removed);
		}
	}

	return Removed;
}

bool UInventoryComponent::MoveItem(const int32 FromSlotIndex, const int32 ToSlotIndex)
{
	if (!IsValidSlotIndex(FromSlotIndex) || !IsValidSlotIndex(ToSlotIndex) || FromSlotIndex == ToSlotIndex || Slots[FromSlotIndex].IsEmpty())
	{
		return false;
	}

	FInventorySlot& From = Slots[FromSlotIndex];
	FInventorySlot& To = Slots[ToSlotIndex];

	if (!To.IsEmpty() && To.Item == From.Item && To.Count < To.Item->GetMaxStackSize())
	{
		const int32 Moved = FMath::Min(To.Item->GetMaxStackSize() - To.Count, From.Count);
		To.Count += Moved;
		From.Count -= Moved;

		if (From.Count <= 0)
		{
			From = FInventorySlot(nullptr, 0);
		}
	}
	else
	{
		Swap(From, To);
	}

	BroadcastSlotChanged(FromSlotIndex);
	BroadcastSlotChanged(ToSlotIndex);
	return true;
}

/* -------------------- Consumables -------------------- */

namespace
{
	// Buffs from consumables are keyed so they show on the active effects bar (unkeyed entries don't).
	const FName ConsumableEffectKey(TEXT("Consumable"));
}

bool UInventoryComponent::CanUseItem(const int32 SlotIndex) const
{
	if (IsSlotEmpty(SlotIndex))
	{
		return false;
	}

	const UConsumableDataAsset* Consumable = Cast<UConsumableDataAsset>(Slots[SlotIndex].Item);

	if (!IsValid(Consumable) || IsItemOnCooldown(Consumable))
	{
		return false;
	}

	if (IsValid(ResourceComponent) && ResourceComponent->IsDead())
	{
		return false;
	}

	return WouldConsumableHaveEffect(Consumable);
}

bool UInventoryComponent::UseItem(const int32 SlotIndex)
{
	if (!CanUseItem(SlotIndex))
	{
		return false;
	}

	UConsumableDataAsset* Consumable = CastChecked<UConsumableDataAsset>(Slots[SlotIndex].Item);

	if (IsValid(ResourceComponent))
	{
		for (const FResourceRestore& Restore : Consumable->GetRestores())
		{
			const float Max = ResourceComponent->GetResourceValue(Restore.Resource, EResourceValueType::Max);
			const float Amount = Restore.Amount + Max * Restore.PercentOfMax / 100.0f;

			if (Amount > 0.0f)
			{
				ResourceComponent->ModifyResource(Restore.Resource, EResourceValueType::Current, Amount);
			}
		}
	}

	if (Consumable->HasTimedEffect() && IsValid(CombatantComponent))
	{
		// The item is the source, so the buff shows the potion's name and icon; drinking another refreshes it.
		CombatantComponent->ApplyEffect(Consumable, ConsumableEffectKey, Consumable->GetStatModifiers(), 1,
			Consumable->GetGrantedTags(), Consumable->GetEffectDuration());
	}

	if (Consumable->GetCooldownDuration() > 0.0f)
	{
		if (IsValid(AbilityComponent))
		{
			AbilityComponent->StartCooldown(Consumable->GetCooldownTag(), Consumable->GetCooldownDuration());
		}
		else
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("%s: no Ability Component, so %s was used without a cooldown."), *GetNameSafe(GetOwner()), *GetNameSafe(Consumable));
		}
	}

	RemoveItemAt(SlotIndex, 1);
	OnItemUsed.Broadcast(Consumable);
	return true;
}

bool UInventoryComponent::UseItemOfType(const UConsumableDataAsset* Item)
{
	if (!IsValid(Item))
	{
		return false;
	}

	// From the back, like Remove Item, so the stacks the player sees first stay full.
	for (int32 SlotIndex = Slots.Num() - 1; SlotIndex >= 0; --SlotIndex)
	{
		if (!Slots[SlotIndex].IsEmpty() && Slots[SlotIndex].Item == Item)
		{
			return UseItem(SlotIndex);
		}
	}

	return false;
}

FGameplayTag UInventoryComponent::GetItemCooldownTag(const UItemDataAsset* Item)
{
	const UConsumableDataAsset* Consumable = Cast<UConsumableDataAsset>(Item);
	return IsValid(Consumable) && Consumable->GetCooldownDuration() > 0.0f ? Consumable->GetCooldownTag() : FGameplayTag();
}

bool UInventoryComponent::IsItemOnCooldown(const UItemDataAsset* Item) const
{
	return GetItemCooldownRemaining(Item) > 0.0f;
}

float UInventoryComponent::GetItemCooldownRemaining(const UItemDataAsset* Item) const
{
	const FGameplayTag CooldownTag = GetItemCooldownTag(Item);
	return CooldownTag.IsValid() && IsValid(AbilityComponent) ? AbilityComponent->GetAbilityCooldownRemaining(CooldownTag) : 0.0f;
}

float UInventoryComponent::GetItemCooldownProgress(const UItemDataAsset* Item) const
{
	const FGameplayTag CooldownTag = GetItemCooldownTag(Item);
	return CooldownTag.IsValid() && IsValid(AbilityComponent) ? AbilityComponent->GetAbilityCooldownProgress(CooldownTag) : 0.0f;
}

bool UInventoryComponent::WouldConsumableHaveEffect(const UConsumableDataAsset* Item) const
{
	if (Item->HasTimedEffect())
	{
		return true;
	}

	if (!IsValid(ResourceComponent))
	{
		return false;
	}

	for (const FResourceRestore& Restore : Item->GetRestores())
	{
		const bool bRestoresSomething = Restore.Amount > 0.0f || Restore.PercentOfMax > 0.0f;
		const float Current = ResourceComponent->GetResourceValue(Restore.Resource, EResourceValueType::Current);
		const float Max = ResourceComponent->GetResourceValue(Restore.Resource, EResourceValueType::Max);

		if (bRestoresSomething && Current < Max)
		{
			return true;
		}
	}

	return false;
}

void UInventoryComponent::HandleCooldownStarted(const FGameplayTag CooldownTag, const float Duration)
{
	OnItemCooldownStarted.Broadcast(CooldownTag, Duration);
}

/* -------------------- Equipment -------------------- */

FEquipmentSlotDefinition UInventoryComponent::GetEquipmentSlotDefinition(const int32 EquipmentSlotIndex) const
{
	return EquipmentSlots.IsValidIndex(EquipmentSlotIndex) ? EquipmentSlots[EquipmentSlotIndex] : FEquipmentSlotDefinition();
}

int32 UInventoryComponent::FindEquipmentSlotIndex(const FGameplayTag SlotTag) const
{
	if (!SlotTag.IsValid())
	{
		return INDEX_NONE;
	}

	return EquipmentSlots.IndexOfByPredicate([SlotTag](const FEquipmentSlotDefinition& Definition)
	{
		return Definition.SlotTag.MatchesTagExact(SlotTag);
	});
}

UEquipmentDataAsset* UInventoryComponent::GetEquippedItem(const int32 EquipmentSlotIndex) const
{
	return Equipped.IsValidIndex(EquipmentSlotIndex) ? Equipped[EquipmentSlotIndex].Get() : nullptr;
}

UEquipmentDataAsset* UInventoryComponent::GetEquippedItemInSlot(const FGameplayTag SlotTag) const
{
	return GetEquippedItem(FindEquipmentSlotIndex(SlotTag));
}

bool UInventoryComponent::CanEquip(const UItemDataAsset* Item, const int32 EquipmentSlotIndex) const
{
	const UEquipmentDataAsset* Equipment = Cast<UEquipmentDataAsset>(Item);

	if (!IsValid(Equipment))
	{
		return false;
	}

	const int32 MatchingIndex = FindEquipmentSlotIndex(Equipment->GetEquipmentSlot());
	return MatchingIndex != INDEX_NONE && (EquipmentSlotIndex == INDEX_NONE || EquipmentSlotIndex == MatchingIndex);
}

bool UInventoryComponent::EquipFromSlot(const int32 SlotIndex, const int32 EquipmentSlotIndex)
{
	if (IsSlotEmpty(SlotIndex) || !CanEquip(Slots[SlotIndex].Item, EquipmentSlotIndex))
	{
		return false;
	}

	UEquipmentDataAsset* Item = CastChecked<UEquipmentDataAsset>(Slots[SlotIndex].Item);
	const int32 TargetIndex = FindEquipmentSlotIndex(Item->GetEquipmentSlot());

	// The bag slot the item leaves is exactly where the old piece goes: a swap never needs free space.
	UEquipmentDataAsset* Previous = SetEquipped(TargetIndex, Item);
	Slots[SlotIndex] = Previous ? FInventorySlot(Previous, 1) : FInventorySlot(nullptr, 0);

	BroadcastSlotChanged(SlotIndex);
	return true;
}

bool UInventoryComponent::Unequip(const int32 EquipmentSlotIndex, int32 SlotIndex)
{
	UEquipmentDataAsset* Item = GetEquippedItem(EquipmentSlotIndex);

	if (!IsValid(Item))
	{
		return false;
	}

	if (SlotIndex == INDEX_NONE)
	{
		SlotIndex = FindFirstEmptySlot();
	}

	if (!IsValidSlotIndex(SlotIndex))
	{
		// Bag full: it stays on.
		return false;
	}

	if (!Slots[SlotIndex].IsEmpty())
	{
		// Dropped on a piece for the same slot: wear that one instead.
		return CanEquip(Slots[SlotIndex].Item, EquipmentSlotIndex) && EquipFromSlot(SlotIndex, EquipmentSlotIndex);
	}

	SetEquipped(EquipmentSlotIndex, nullptr);
	Slots[SlotIndex] = FInventorySlot(Item, 1);

	BroadcastSlotChanged(SlotIndex);
	return true;
}

UEquipmentDataAsset* UInventoryComponent::SetEquipped(const int32 EquipmentSlotIndex, UEquipmentDataAsset* Item)
{
	if (!Equipped.IsValidIndex(EquipmentSlotIndex))
	{
		return nullptr;
	}

	UEquipmentDataAsset* Previous = Equipped[EquipmentSlotIndex];

	if (Previous == Item)
	{
		return nullptr;
	}

	RemoveStats(Previous);
	Equipped[EquipmentSlotIndex] = Item;
	ApplyStats(Item);

	RefreshEquipmentMeshes();

	OnEquipmentChanged.Broadcast(EquipmentSlotIndex, Previous, Item);
	return Previous;
}

void UInventoryComponent::ApplyStats(const UEquipmentDataAsset* Item) const
{
	if (!IsValid(Item) || !IsValid(CombatantComponent) || (Item->GetStatModifiers().IsEmpty() && Item->GetGrantedTags().IsEmpty()))
	{
		return;
	}

	// Unkeyed: equipment bonuses are permanent while worn, so they stay off the active effects bar.
	// The item is the source; each item fits one slot, so it can't be worn twice and collide with itself.
	CombatantComponent->ApplyEffect(Item, NAME_None, Item->GetStatModifiers(), 1, Item->GetGrantedTags(), 0.0f);
}

void UInventoryComponent::RemoveStats(const UEquipmentDataAsset* Item) const
{
	if (IsValid(Item) && IsValid(CombatantComponent))
	{
		CombatantComponent->RemoveEffect(Item, NAME_None);
	}
}

/* -------------------- Meshes -------------------- */

void UInventoryComponent::RegisterEquipmentMeshes(const TArray<FEquipmentMeshBinding>& Bindings)
{
	for (const FEquipmentMeshBinding& Binding : Bindings)
	{
		if (!Binding.SlotTag.IsValid() || !IsValid(Binding.MeshComponent))
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("%s: skipped an equipment mesh binding with no slot tag or mesh component."), *GetNameSafe(GetOwner()));
			continue;
		}

		FRegisteredEquipmentMesh& Registered = EquipmentMeshes.FindOrAdd(Binding.SlotTag);

		// Registered twice: it may be wearing something now, so keep the default from the first time.
		if (Registered.Component == Binding.MeshComponent)
		{
			continue;
		}

		// Its current mesh is what it goes back to when nothing is worn there.
		Registered.Component = Binding.MeshComponent;
		Registered.DefaultMesh = Binding.MeshComponent->GetSkeletalMeshAsset();
	}

	// Registering after BeginPlay (the usual case) shows any starting equipment straight away.
	RefreshEquipmentMeshes();
}

void UInventoryComponent::RefreshEquipmentMeshes()
{
	// What every registered part should show: its default, then each worn item overrides in slot order.
	// Rebuilding from scratch means taking off armor that replaced the chest piece brings it back,
	// whatever order things were put on.
	TMap<FGameplayTag, USkeletalMesh*> Wanted;

	for (const TPair<FGameplayTag, FRegisteredEquipmentMesh>& Registered : EquipmentMeshes)
	{
		Wanted.Add(Registered.Key, Registered.Value.DefaultMesh);
	}

	for (int32 EquipmentSlotIndex = 0; EquipmentSlotIndex < Equipped.Num(); ++EquipmentSlotIndex)
	{
		const UEquipmentDataAsset* Item = Equipped[EquipmentSlotIndex];

		if (!IsValid(Item))
		{
			continue;
		}

		for (const FEquipmentMesh& Part : Item->GetMeshes())
		{
			const FGameplayTag PartTag = Part.Part.IsValid() ? Part.Part : Item->GetEquipmentSlot();

			// Not registered (yet): nothing to show it on. Registering later picks it up.
			if (Wanted.Contains(PartTag))
			{
				Wanted.Add(PartTag, Part.Mesh);
			}
		}
	}

	for (const TPair<FGameplayTag, USkeletalMesh*>& Entry : Wanted)
	{
		USkeletalMeshComponent* Component = EquipmentMeshes.FindChecked(Entry.Key).Component;

		if (IsValid(Component) && Component->GetSkeletalMeshAsset() != Entry.Value)
		{
			Component->SetSkeletalMeshAsset(Entry.Value);
		}
	}
}

void UInventoryComponent::BroadcastSlotChanged(const int32 SlotIndex)
{
	OnInventorySlotChanged.Broadcast(SlotIndex);
}

#if WITH_EDITOR
EDataValidationResult UInventoryComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TSet<FGameplayTag> SeenTags;

	for (int32 Index = 0; Index < EquipmentSlots.Num(); ++Index)
	{
		const FGameplayTag SlotTag = EquipmentSlots[Index].SlotTag;

		if (!SlotTag.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("MissingSlotTag", "Equipment slot {0} has no Slot Tag."), Index));
			Result = EDataValidationResult::Invalid;
		}
		else if (SeenTags.Contains(SlotTag))
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateSlotTag", "Equipment slot tag {0} is used twice."), FText::FromName(SlotTag.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}

		SeenTags.Add(SlotTag);
	}

	for (const UEquipmentDataAsset* Item : StartingEquipment)
	{
		if (IsValid(Item) && FindEquipmentSlotIndex(Item->GetEquipmentSlot()) == INDEX_NONE)
		{
			Context.AddError(FText::Format(LOCTEXT("StartingNoSlot", "Starting equipment {0} has no matching equipment slot."), FText::FromString(Item->GetName())));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
