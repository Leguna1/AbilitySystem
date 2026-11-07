#include "LootPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "InventoryComponent.h"
#include "ItemDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ALootPickup::ALootPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(80.0f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
	SetRootComponent(PickupSphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(PickupSphere);
	// Lifted off the ground point the drop was traced to, so it doesn't sink into slopes.
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	// Something visible for items without a Pickup Mesh, until a Blueprint child sets its own.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FallbackMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (FallbackMesh.Succeeded())
	{
		Mesh->SetStaticMesh(FallbackMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.25f));
	}

	Spin = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spin"));
	Spin->RotationRate = FRotator(0.0f, 90.0f, 0.0f);
	Spin->SetUpdatedComponent(Mesh);
}

void ALootPickup::BeginPlay()
{
	Super::BeginPlay();

	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ALootPickup::HandleBeginOverlap);

	if (Lifetime > 0.0f)
	{
		SetLifeSpan(Lifetime);
	}

	if (PickupDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(PickupDelayTimer, this, &ALootPickup::EnablePickup, PickupDelay, false);
	}
	else
	{
		EnablePickup();
	}
}

void ALootPickup::InitializePickup(UItemDataAsset* InItem, const int32 InCount)
{
	Item = InItem;
	Count = FMath::Max(InCount, 0);

	if (IsValid(Item) && IsValid(Item->GetPickupMesh()))
	{
		Mesh->SetStaticMesh(Item->GetPickupMesh());
	}

	OnPickupInitialized(Item, Count);
}

bool ALootPickup::TryPickUp(AActor* Collector)
{
	if (!bCanBePickedUp || !IsValid(Item) || Count <= 0 || !IsValid(Collector))
	{
		return false;
	}

	UInventoryComponent* Inventory = Collector->FindComponentByClass<UInventoryComponent>();

	if (!IsValid(Inventory))
	{
		return false;
	}

	const int32 Leftover = Inventory->AddItem(Item, Count);
	const int32 Taken = Count - Leftover;

	if (Taken <= 0)
	{
		return false;
	}

	Count = Leftover;

	if (IsValid(PickupSound))
	{
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	OnPickedUp(Collector, Taken);

	if (Count > 0)
	{
		return false;
	}

	Destroy();
	return true;
}

void ALootPickup::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryPickUp(OtherActor);
}

void ALootPickup::EnablePickup()
{
	bCanBePickedUp = true;

	TArray<AActor*> Overlapping;
	PickupSphere->GetOverlappingActors(Overlapping);

	for (AActor* Collector : Overlapping)
	{
		if (TryPickUp(Collector))
		{
			return;
		}
	}
}
