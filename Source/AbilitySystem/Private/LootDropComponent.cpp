#include "LootDropComponent.h"

#include "AbilitySystemLog.h"
#include "AbilitySystemSettings.h"
#include "CombatantComponent.h"
#include "Engine/World.h"
#include "ItemDataAsset.h"
#include "LootPickup.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "LootDropComponent"

ULootDropComponent::ULootDropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void ULootDropComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bDropOnDeath)
	{
		if (UCombatantComponent* Combatant = GetOwner()->FindComponentByClass<UCombatantComponent>())
		{
			Combatant->OnDied.AddUniqueDynamic(this, &ULootDropComponent::HandleOwnerDied);
		}
		else
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("%s: Loot Drop Component has Drop On Death on but no Combatant Component to hear the death."), *GetNameSafe(GetOwner()));
		}
	}
}

void ULootDropComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCombatantComponent* Combatant = GetOwner()->FindComponentByClass<UCombatantComponent>())
	{
		Combatant->OnDied.RemoveDynamic(this, &ULootDropComponent::HandleOwnerDied);
	}

	Super::EndPlay(EndPlayReason);
}

void ULootDropComponent::HandleOwnerDied(AActor* Killer)
{
	DropLoot();
}

float ULootDropComponent::GetFinalDropChance(const FLootDrop& Entry) const
{
	if (!IsValid(Entry.Item))
	{
		return 0.0f;
	}

	const float EntryChance = Entry.bOverrideDropChance
		? Entry.DropChance
		: Entry.Item->GetDropChance() * Entry.DropChanceMultiplier;

	return FMath::Clamp(EntryChance * LootChanceMultiplier, 0.0f, 100.0f);
}

TArray<FInventorySlot> ULootDropComponent::RollLoot() const
{
	TArray<FInventorySlot> Results;

	for (const FLootDrop& Entry : Loot)
	{
		const float Chance = GetFinalDropChance(Entry);

		// FRand is [0, 1), so 100% always passes and 0% never does.
		if (Chance <= 0.0f || FMath::FRand() * 100.0f >= Chance)
		{
			continue;
		}

		const int32 MinCount = FMath::Max(Entry.MinCount, 1);
		const int32 DropCount = FMath::RandRange(MinCount, FMath::Max(Entry.MaxCount, MinCount));
		Results.Emplace(Entry.Item, DropCount);
	}

	return Results;
}

TArray<ALootPickup*> ULootDropComponent::DropLoot()
{
	TArray<ALootPickup*> Pickups;
	const FVector Origin = GetOwner()->GetActorLocation();

	for (const FInventorySlot& Drop : RollLoot())
	{
		if (ALootPickup* Pickup = SpawnPickup(Drop.Item, Drop.Count, Origin))
		{
			Pickups.Add(Pickup);
		}
	}

	if (!Pickups.IsEmpty())
	{
		OnLootDropped.Broadcast(Pickups);
	}

	return Pickups;
}

ALootPickup* ULootDropComponent::SpawnPickup(UItemDataAsset* Item, const int32 Count, const FVector Origin)
{
	UWorld* World = GetWorld();
	const TSubclassOf<ALootPickup> SpawnClass = ResolvePickupClass();

	if (!IsValid(World) || !IsValid(Item) || Count <= 0 || !SpawnClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FRotator RandomYaw(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
	ALootPickup* Pickup = World->SpawnActor<ALootPickup>(SpawnClass, FindDropLocation(Origin), RandomYaw, SpawnParams);

	if (IsValid(Pickup))
	{
		Pickup->InitializePickup(Item, Count);
	}

	return Pickup;
}

TSubclassOf<ALootPickup> ULootDropComponent::ResolvePickupClass() const
{
	if (PickupClass)
	{
		return PickupClass;
	}

	// Loaded on first drop; a pickup Blueprint is small, and this keeps it out of memory in levels without loot.
	if (UClass* DefaultClass = GetDefault<UAbilitySystemSettings>()->DefaultLootPickupClass.LoadSynchronous())
	{
		return DefaultClass;
	}

	return ALootPickup::StaticClass();
}

FVector ULootDropComponent::FindDropLocation(const FVector& Origin) const
{
	const FVector2D Offset = FMath::RandPointInCircle(ScatterRadius);
	const FVector Above = Origin + FVector(Offset.X, Offset.Y, 100.0f);
	const FVector Below = Above - FVector(0.0f, 0.0f, 600.0f);

	// Static world only: the corpse, the player and other pickups would otherwise catch the trace.
	FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(LootDropTrace), false, GetOwner());

	FHitResult Hit;

	if (GetWorld()->LineTraceSingleByObjectType(Hit, Above, Below, ObjectParams, QueryParams))
	{
		return Hit.ImpactPoint;
	}

	// No ground found (over a ledge): drop it at the owner's own height rather than losing it.
	return FVector(Above.X, Above.Y, Origin.Z);
}

#if WITH_EDITOR
EDataValidationResult ULootDropComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (int32 Index = 0; Index < Loot.Num(); ++Index)
	{
		const FLootDrop& Entry = Loot[Index];

		if (!IsValid(Entry.Item))
		{
			Context.AddError(FText::Format(LOCTEXT("MissingItem", "Loot[{0}] has no Item."), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (Entry.MaxCount < Entry.MinCount)
		{
			Context.AddError(FText::Format(LOCTEXT("BadCount", "Loot[{0}] Max Count is smaller than Min Count."), Index));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
