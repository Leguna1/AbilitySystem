#include "Breakable.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "LootDropComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ABreakable::ABreakable()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	// Projectiles hit by overlapping.
	Mesh->SetGenerateOverlapEvents(true);

	// Something visible until a Blueprint child sets its own mesh.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	if (FallbackMesh.Succeeded())
	{
		Mesh->SetStaticMesh(FallbackMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.7f));
	}

	LootDropComponent = CreateDefaultSubobject<ULootDropComponent>(TEXT("LootDropComponent"));
	LootDropComponent->SetDropOnDeath(false);
}

void ABreakable::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(RespawnTimer);
	Super::EndPlay(EndPlayReason);
}

bool ABreakable::ReceivePayload_Implementation(const FAbilityPayload& Payload)
{
	if (bBroken || Payload.Damage <= 0.0f)
	{
		return false;
	}

	if (++HitsTaken >= HitsToBreak)
	{
		Break(Payload.Instigator.Get());
	}

	return true;
}

void ABreakable::Break(AActor* Breaker)
{
	bBroken = true;

	LootDropComponent->DropLoot();
	OnBroken(Breaker);

	if (RespawnDelay <= 0.0f)
	{
		// A moment for Blueprint effects started in On Broken to spawn from this actor.
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetLifeSpan(1.0f);
		return;
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ABreakable::Restore, RespawnDelay, false);
}

void ABreakable::Restore()
{
	bBroken = false;
	HitsTaken = 0;

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	OnRestored();
}
