#include "EnemySpawner.h"

#include "CombatantComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

namespace
{
	constexpr float UpdateInterval = 0.5f;
}

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnArea = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnArea"));
	SetRootComponent(SpawnArea);

	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpawnArea->SetHiddenInGame(true);
	SpawnArea->ShapeColor = FColor::Magenta;
	SpawnArea->SetSphereRadius(SpawnRadius);
}

void AEnemySpawner::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (IsValid(SpawnArea))
	{
		SpawnArea->SetSphereRadius(FMath::Max(SpawnRadius, 50.0f));
	}
}

void AEnemySpawner::BeginPlay()
{
	Super::BeginPlay();

	Slots.SetNum(Count);

	if (bSpawnOnBeginPlay)
	{
		SpawnMissing();
	}

	GetWorldTimerManager().SetTimer(UpdateTimer, this, &AEnemySpawner::UpdateSlots, UpdateInterval, true);
}

void AEnemySpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(UpdateTimer);
	Super::EndPlay(EndPlayReason);
}

void AEnemySpawner::SpawnMissing()
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (!IsEnemyAlive(Slots[SlotIndex].Enemy.Get()))
		{
			SpawnInSlot(SlotIndex);
		}
	}
}

int32 AEnemySpawner::GetAliveCount() const
{
	int32 Alive = 0;

	for (const FSpawnSlot& Slot : Slots)
	{
		Alive += IsEnemyAlive(Slot.Enemy.Get()) ? 1 : 0;
	}

	return Alive;
}

void AEnemySpawner::UpdateSlots()
{
	const double Now = GetWorld()->GetTimeSeconds();
	bool bAllDead = !Slots.IsEmpty();
	double LastDeath = 0.0;

	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		FSpawnSlot& Slot = Slots[SlotIndex];
		APawn* SlotEnemy = Slot.Enemy.Get();

		if (IsEnemyAlive(SlotEnemy))
		{
			bAllDead = false;
			continue;
		}

		// Just died (or was removed by something else).
		if (Slot.DiedAt < 0.0)
		{
			Slot.DiedAt = Now;

			if (IsValid(SlotEnemy))
			{
				OnEnemyDied(SlotEnemy);
			}
		}

		LastDeath = FMath::Max(LastDeath, Slot.DiedAt);

		if (CorpseLifetime > 0.0f && IsValid(SlotEnemy) && Now - Slot.DiedAt >= CorpseLifetime)
		{
			SlotEnemy->Destroy();
		}

		if (!bRespawnAsGroup && RespawnDelay > 0.0f && Now - Slot.DiedAt >= RespawnDelay)
		{
			SpawnInSlot(SlotIndex);
		}
	}

	// The whole pack returns together, counted from the last one that fell.
	if (bRespawnAsGroup && bAllDead && RespawnDelay > 0.0f && Now - LastDeath >= RespawnDelay)
	{
		SpawnMissing();
	}
}

void AEnemySpawner::SpawnInSlot(const int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex) || !EnemyClass)
	{
		return;
	}

	FSpawnSlot& Slot = Slots[SlotIndex];

	// A corpse still lying around makes room for its replacement.
	if (APawn* OldEnemy = Slot.Enemy.Get())
	{
		OldEnemy->Destroy();
	}

	Slot.Enemy.Reset();
	Slot.DiedAt = -1.0;

	FVector Location;

	if (!FindSpawnLocation(Location))
	{
		return;
	}

	const FRotator Rotation(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	APawn* SpawnedEnemy = GetWorld()->SpawnActor<APawn>(EnemyClass, Location, Rotation, Params);

	if (!IsValid(SpawnedEnemy))
	{
		return;
	}

	// Placed enemies get their AI automatically; spawned ones only if their class says so.
	if (!SpawnedEnemy->GetController())
	{
		SpawnedEnemy->SpawnDefaultController();
	}

	Slot.Enemy = SpawnedEnemy;
	OnEnemySpawned(SpawnedEnemy);
}

bool AEnemySpawner::FindSpawnLocation(FVector& OutLocation) const
{
	const FVector Origin = GetActorLocation();
	OutLocation = Origin;

		if (const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
		FNavLocation NavLocation;

		if (NavSystem->GetRandomReachablePointInRadius(Origin, SpawnRadius, NavLocation))
		{
			OutLocation = NavLocation.Location;
		}
	}

	// The navmesh point is on the floor; a character's origin is at its capsule's center.
	if (const ACharacter* Defaults = Cast<ACharacter>(EnemyClass->GetDefaultObject()))
	{
		if (const UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent())
		{
			OutLocation.Z += Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	return true;
}

bool AEnemySpawner::IsEnemyAlive(const APawn* SpawnedEnemy)
{
	if (!IsValid(SpawnedEnemy))
	{
		return false;
	}

	const UCombatantComponent* Combatant = SpawnedEnemy->FindComponentByClass<UCombatantComponent>();
	return !IsValid(Combatant) || !Combatant->IsDead();
}
