#include "WeaponManagerComponent.h"

#include "AbilityComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "InventoryComponent.h"
#include "TimerManager.h"
#include "WeaponDataAsset.h"

UWeaponManagerComponent::UWeaponManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningCharacter = Cast<ACharacter>(GetOwner());
	AbilityComponent = IsValid(GetOwner())
		? GetOwner()->FindComponentByClass<UAbilityComponent>()
		: nullptr;

	if (!IsValid(OwningCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("UWeaponManagerComponent requires an ACharacter owner."));
		return;
	}

	if (!IsValid(AbilityComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("UWeaponManagerComponent on %s found no UAbilityComponent; weapon kits will not be granted."),
			*GetNameSafe(GetOwner()));
	}

	if (!InventoryWeaponSlots.IsEmpty())
	{
		if (UInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UInventoryComponent>())
		{
			BoundInventory = Inventory;
			Loadout.Init(nullptr, InventoryWeaponSlots.Num());
			Inventory->OnEquipmentChanged.AddUniqueDynamic(this, &UWeaponManagerComponent::HandleEquipmentChanged);

			for (const FGameplayTag& SlotTag : InventoryWeaponSlots)
			{
				if (Inventory->FindEquipmentSlotIndex(SlotTag) == INDEX_NONE)
				{
					UE_LOG(LogTemp, Warning, TEXT("%s: Inventory Weapon Slot %s isn't one of the inventory's Equipment Slots; nothing can be equipped there."),
						*GetNameSafe(GetOwner()), *SlotTag.ToString());
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: Inventory Weapon Slots is set but there is no Inventory Component; using Loadout."), *GetNameSafe(GetOwner()));
		}
	}

	bStartingDrawPending = StartingLoadoutIndex != INDEX_NONE;

	// The inventory may not have begun play yet: its starting weapons then arrive through On Equipment Changed.
	if (IsValid(BoundInventory))
	{
		SyncLoadoutFromInventory();
	}

	TryStartingDraw();

	// Starting equipment arrives during this frame's Begin Plays (ours or the inventory's) and follows
	// Starting Loadout Index. From the next frame on, equipping is the player's doing.
	if (IsValid(BoundInventory) && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			bStartupEquipDone = true;
		}));
	}
}

void UWeaponManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(BoundInventory))
	{
		BoundInventory->OnEquipmentChanged.RemoveDynamic(this, &UWeaponManagerComponent::HandleEquipmentChanged);
		BoundInventory = nullptr;
	}

	RevokeEquippedKit();

	EquippedWeapon = nullptr;
	EquippedLoadoutIndex = INDEX_NONE;

	DestroyCachedWeapons();

	Super::EndPlay(EndPlayReason);
}

bool UWeaponManagerComponent::EquipWeapon(UWeaponDataAsset* WeaponData)
{
	if (!IsValid(WeaponData))
	{
		return false;
	}

	return SwapTo(WeaponData, Loadout.IndexOfByKey(WeaponData));
}

bool UWeaponManagerComponent::EquipLoadoutIndex(const int32 LoadoutIndex)
{
	if (!Loadout.IsValidIndex(LoadoutIndex) || !IsValid(Loadout[LoadoutIndex]))
	{
		return false;
	}

	return SwapTo(Loadout[LoadoutIndex], LoadoutIndex);
}

bool UWeaponManagerComponent::CycleWeapon(const int32 Direction)
{
	const int32 Count = Loadout.Num();

	if (Count == 0 || Direction == 0)
	{
		return false;
	}

	const int32 Step = Direction > 0 ? 1 : -1;
	int32 Candidate = EquippedLoadoutIndex;

	for (int32 Attempt = 0; Attempt < Count; ++Attempt)
	{
		Candidate = Candidate == INDEX_NONE
			? (Step > 0 ? 0 : Count - 1)
			: (Candidate + Step + Count) % Count;

		if (Candidate == EquippedLoadoutIndex)
		{
			return false;
		}

		if (IsValid(Loadout[Candidate]))
		{
			// A refused swap (e.g. committed ability) applies to every candidate, so stop here.
			return EquipLoadoutIndex(Candidate);
		}
	}

	return false;
}

bool UWeaponManagerComponent::HolsterWeapon()
{
	return IsValid(EquippedWeapon) && SwapTo(nullptr, INDEX_NONE);
}

bool UWeaponManagerComponent::CanSwapWeapon() const
{
	return !IsValid(AbilityComponent) || !AbilityComponent->HasActiveAbility();
}

bool UWeaponManagerComponent::UsesInventoryLoadout() const
{
	return IsValid(BoundInventory);
}

bool UWeaponManagerComponent::CanChangeWeaponSlot(const FGameplayTag& SlotTag) const
{
	if (!IsValid(BoundInventory))
	{
		return true;
	}

	const int32 LoadoutIndex = InventoryWeaponSlots.IndexOfByKey(SlotTag);
	return LoadoutIndex == INDEX_NONE || LoadoutIndex != EquippedLoadoutIndex || CanSwapWeapon();
}

/* -------------------- Inventory loadout -------------------- */

void UWeaponManagerComponent::HandleEquipmentChanged(const int32 EquipmentSlotIndex, UEquipmentDataAsset* PreviousItem, UEquipmentDataAsset* NewItem)
{
	if (!IsValid(BoundInventory))
	{
		return;
	}

	const FGameplayTag SlotTag = BoundInventory->GetEquipmentSlotDefinition(EquipmentSlotIndex).SlotTag;
	const int32 LoadoutIndex = InventoryWeaponSlots.IndexOfByKey(SlotTag);

	if (LoadoutIndex != INDEX_NONE)
	{
		// Gear put in a weapon slot by mistake counts as no weapon.
		SetLoadoutEntry(LoadoutIndex, Cast<UWeaponDataAsset>(NewItem));
		TryStartingDraw();
	}
}

void UWeaponManagerComponent::SyncLoadoutFromInventory()
{
	for (int32 LoadoutIndex = 0; LoadoutIndex < InventoryWeaponSlots.Num(); ++LoadoutIndex)
	{
		SetLoadoutEntry(LoadoutIndex, Cast<UWeaponDataAsset>(BoundInventory->GetEquippedItemInSlot(InventoryWeaponSlots[LoadoutIndex])));
	}
}

void UWeaponManagerComponent::SetLoadoutEntry(const int32 LoadoutIndex, UWeaponDataAsset* WeaponData)
{
	if (!Loadout.IsValidIndex(LoadoutIndex) || Loadout[LoadoutIndex] == WeaponData)
	{
		return;
	}

	UWeaponDataAsset* PreviousData = Loadout[LoadoutIndex];
	Loadout[LoadoutIndex] = WeaponData;

	if (LoadoutIndex == EquippedLoadoutIndex)
	{
		// The drawn weapon was replaced or taken off: draw the new one in its place, or go unarmed.
		// The inventory checked Can Change Weapon Slot first, so this isn't refused for an active ability.
		const bool bSwapped = IsValid(WeaponData) ? SwapTo(WeaponData, LoadoutIndex) : SwapTo(nullptr, INDEX_NONE);

		if (!bSwapped)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: couldn't swap the drawn weapon to %s after an inventory change."),
				*GetNameSafe(GetOwner()), *GetNameSafe(WeaponData));
		}
	}
	else if (IsValid(WeaponData))
	{
		// Equipped while unarmed: into the hands. Otherwise it stays holstered, which is invisible for a
		// weapon without a Holster Socket Name: equipping would look like it did nothing.
		const bool bDrawNow = bStartupEquipDone && bDrawOnEquipWhenUnarmed && !HasEquippedWeapon() && CanSwapWeapon();

		if (!bDrawNow || !SwapTo(WeaponData, LoadoutIndex))
		{
			FindOrSpawnWeapon(WeaponData);
		}
	}

	ReleaseWeaponIfUnused(PreviousData);
}

void UWeaponManagerComponent::ReleaseWeaponIfUnused(const UWeaponDataAsset* WeaponData)
{
	if (!IsValid(WeaponData) || Loadout.Contains(WeaponData) || GetEquippedWeaponData() == WeaponData)
	{
		return;
	}

	// Back in the bag: no actor left on the character's back.
	for (int32 EntryIndex = CachedWeapons.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		if (CachedWeapons[EntryIndex].WeaponData == WeaponData)
		{
			if (IsValid(CachedWeapons[EntryIndex].Weapon))
			{
				CachedWeapons[EntryIndex].Weapon->Destroy();
			}

			CachedWeapons.RemoveAt(EntryIndex);
		}
	}
}

void UWeaponManagerComponent::TryStartingDraw()
{
	if (bStartingDrawPending && Loadout.IsValidIndex(StartingLoadoutIndex) && IsValid(Loadout[StartingLoadoutIndex]))
	{
		bStartingDrawPending = false;
		EquipLoadoutIndex(StartingLoadoutIndex);
	}
}

/* -------------------- Swapping -------------------- */

bool UWeaponManagerComponent::SwapTo(UWeaponDataAsset* WeaponData, const int32 LoadoutIndex)
{
	if (bSwapping || !IsValid(OwningCharacter))
	{
		return false;
	}

	UWeaponDataAsset* PreviousData = GetEquippedWeaponData();

	// Already drawn (possibly reached through a different loadout entry).
	if (IsValid(WeaponData) && WeaponData == PreviousData)
	{
		EquippedLoadoutIndex = LoadoutIndex;
		return true;
	}

	if (!CanSwapWeapon())
	{
		return false;
	}

	TGuardValue<bool> SwapGuard(bSwapping, true);

	// Prepare the incoming weapon first: if this fails, nothing has changed.
	AWeaponBase* NewWeapon = nullptr;

	if (IsValid(WeaponData))
	{
		NewWeapon = FindOrSpawnWeapon(WeaponData);

		if (!IsValid(NewWeapon))
		{
			return false;
		}
	}

	// Out with the old. No ability can be active here, so revoking only drops
	// grants and tags. The buffer is cleared because input tags are about to map
	// to a different kit (a press that failed on cooldown could still be waiting).
	RevokeEquippedKit();

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->FlushActivationRequests();
	}

	if (IsValid(EquippedWeapon))
	{
		EquippedWeapon->HandleHolstered();
	}

	// In with the new.
	EquippedWeapon = NewWeapon;
	EquippedLoadoutIndex = IsValid(NewWeapon) ? LoadoutIndex : INDEX_NONE;

	if (IsValid(NewWeapon))
	{
		NewWeapon->HandleDrawn();
		GrantKit(WeaponData);
	}

	OnWeaponChanged.Broadcast(PreviousData, NewWeapon);
	return true;
}

AWeaponBase* UWeaponManagerComponent::FindCachedWeapon(const UWeaponDataAsset* WeaponData) const
{
	for (const FWeaponInstanceEntry& Entry : CachedWeapons)
	{
		if (Entry.WeaponData == WeaponData && IsValid(Entry.Weapon))
		{
			return Entry.Weapon;
		}
	}

	return nullptr;
}

AWeaponBase* UWeaponManagerComponent::FindOrSpawnWeapon(UWeaponDataAsset* WeaponData)
{
	if (AWeaponBase* Cached = FindCachedWeapon(WeaponData))
	{
		return Cached;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World) || !IsValid(WeaponData) || !WeaponData->WeaponClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwningCharacter;
	SpawnParameters.Instigator = OwningCharacter;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponBase* NewWeapon = World->SpawnActor<AWeaponBase>(
		WeaponData->WeaponClass,
		OwningCharacter->GetActorTransform(),
		SpawnParameters
	);

	if (!IsValid(NewWeapon))
	{
		return nullptr;
	}

	NewWeapon->InitializeWeapon(OwningCharacter, WeaponData);

	// Initial placement: holster socket, or hidden until drawn.
	NewWeapon->HandleHolstered();

	// Drop any stale entry for this asset (weapon destroyed externally) before caching.
	CachedWeapons.RemoveAll([WeaponData](const FWeaponInstanceEntry& Entry)
	{
		return Entry.WeaponData == WeaponData;
	});

	FWeaponInstanceEntry& Entry = CachedWeapons.AddDefaulted_GetRef();
	Entry.WeaponData = WeaponData;
	Entry.Weapon = NewWeapon;

	return NewWeapon;
}

void UWeaponManagerComponent::GrantKit(const UWeaponDataAsset* WeaponData)
{
	if (!IsValid(AbilityComponent) || !IsValid(WeaponData))
	{
		return;
	}

	// Tags first, so anything reacting to the grant event already sees them.
	for (const FGameplayTag& Tag : WeaponData->GrantedOwnerTags)
	{
		AbilityComponent->AddLooseOwnerTag(Tag);
	}

	AbilityComponent->GrantAbilities(WeaponData->AbilityKit, this);
}

void UWeaponManagerComponent::RevokeEquippedKit()
{
	if (!IsValid(AbilityComponent))
	{
		return;
	}

	AbilityComponent->RevokeAbilitiesFromSource(this);

	if (const UWeaponDataAsset* WeaponData = GetEquippedWeaponData())
	{
		for (const FGameplayTag& Tag : WeaponData->GrantedOwnerTags)
		{
			AbilityComponent->RemoveLooseOwnerTag(Tag);
		}
	}
}

void UWeaponManagerComponent::DestroyCachedWeapons()
{
	for (const FWeaponInstanceEntry& Entry : CachedWeapons)
	{
		if (IsValid(Entry.Weapon))
		{
			Entry.Weapon->Destroy();
		}
	}

	CachedWeapons.Reset();
}