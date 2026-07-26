#include "BowBase.h"

#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

ABowBase::ABowBase()
{
	PrimaryActorTick.bCanEverTick = false;

	BowMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BowMesh"));
	SetRootComponent(BowMesh);
}

void ABowBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeArrowPool();
}

void ABowBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndDrawVisuals();
	DestroyArrowPool();

	Super::EndPlay(EndPlayReason);
}

void ABowBase::BeginDrawVisuals_Implementation()
{
	if (bDrawVisualsActive)
	{
		return;
	}

	bDrawVisualsActive = true;

	if (!IsValid(DrawSound))
	{
		return;
	}

	if (IsValid(DrawSoundRef))
	{
		DrawSoundRef->Stop();
		DrawSoundRef = nullptr;
	}

	DrawSoundRef = UGameplayStatics::SpawnSoundAttached(DrawSound, BowMesh);
}

void ABowBase::EndDrawVisuals_Implementation()
{
	bDrawVisualsActive = false;

	if (!IsValid(DrawSoundRef))
	{
		return;
	}

	DrawSoundRef->Stop();
	DrawSoundRef = nullptr;
}

void ABowBase::SetWielderMesh(USkeletalMeshComponent* InWielderMesh)
{
	WielderMesh = InWielderMesh;
}

bool ABowBase::PrepareArrow(const FArrowStats& ArrowStats)
{
	if (IsValid(PreparedArrow))
	{
		return false;
	}

	PreparedArrow = AcquireAvailableArrow();

	if (!IsValid(PreparedArrow))
	{
		return false;
	}

	PreparedArrow->ActivateFromPool(ArrowStats);
	return true;
}

bool ABowBase::AttachPreparedArrowToWielder(const FName SocketName, const FTransform& RelativeOffset)
{
	if (!IsValid(PreparedArrow) ||
		!IsValid(WielderMesh) ||
		SocketName.IsNone() ||
		!WielderMesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	if (!PreparedArrow->AttachToComponent(
		WielderMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName))
	{
		return false;
	}

	PreparedArrow->SetActorRelativeTransform(RelativeOffset);
	return true;
}

bool ABowBase::AttachPreparedArrowToBow(const FName SocketName, const FTransform& RelativeOffset)
{
	if (!IsValid(PreparedArrow) ||
		!IsValid(BowMesh) ||
		SocketName.IsNone() ||
		!BowMesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	if (!PreparedArrow->AttachToComponent(
		BowMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName))
	{
		return false;
	}

	PreparedArrow->SetActorRelativeTransform(RelativeOffset);
	return true;
}

bool ABowBase::ReleasePreparedArrow(const FVector& Direction, const float Strength)
{
	if (!IsValid(PreparedArrow))
	{
		return false;
	}

	const FVector NormalizedDirection = Direction.GetSafeNormal();

	if (NormalizedDirection.IsNearlyZero())
	{
		return false;
	}

	AArrowBase* ArrowToFire = PreparedArrow;
	const float ClampedStrength = FMath::Clamp(Strength, 0.0f, 1.0f);

	ArrowToFire->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (!ArrowToFire->Fire(NormalizedDirection, ClampedStrength))
	{
		return false;
	}

	PreparedArrow = nullptr;
	LastFiredArrow = ArrowToFire;

	OnArrowFired.Broadcast(ArrowToFire, ClampedStrength);
	return true;
}

void ABowBase::DiscardPreparedArrow()
{
	if (!IsValid(PreparedArrow))
	{
		return;
	}

	AArrowBase* ArrowToDiscard = PreparedArrow;
	PreparedArrow = nullptr;

	ArrowToDiscard->ReturnToPool();
}

void ABowBase::InitializeArrowPool()
{
	DiscardPreparedArrow();
	DestroyArrowPool();

	if (!ArrowClass)
	{
		return;
	}

	AllSpawnedArrows.Reserve(InitialArrowPoolSize);
	AvailableArrows.Reserve(InitialArrowPoolSize);

	for (int32 Index = 0; Index < InitialArrowPoolSize; ++Index)
	{
		AArrowBase* NewArrow = CreateArrow();

		if (IsValid(NewArrow))
		{
			AvailableArrows.Add(NewArrow);
		}
	}
}

AArrowBase* ABowBase::CreateArrow()
{
	UWorld* World = GetWorld();

	if (!IsValid(World) || !ArrowClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = GetInstigator();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AArrowBase* NewArrow = World->SpawnActor<AArrowBase>(ArrowClass, FTransform::Identity, SpawnParameters);

	if (!IsValid(NewArrow))
	{
		return nullptr;
	}

	NewArrow->OnReadyToRecycle.AddDynamic(this, &ABowBase::HandleArrowReadyToRecycle);
	NewArrow->ResetForPool();

	AllSpawnedArrows.Add(NewArrow);
	return NewArrow;
}

AArrowBase* ABowBase::AcquireAvailableArrow()
{
	while (!AvailableArrows.IsEmpty())
	{
		AArrowBase* Candidate = AvailableArrows.Pop();

		if (IsValid(Candidate))
		{
			return Candidate;
		}
	}

	return CreateArrow();
}

void ABowBase::DestroyArrowPool()
{
	PreparedArrow = nullptr;
	LastFiredArrow = nullptr;

	for (const TObjectPtr<AArrowBase>& Arrow : AllSpawnedArrows)
	{
		if (!IsValid(Arrow))
		{
			continue;
		}

		Arrow->OnReadyToRecycle.RemoveDynamic(this, &ABowBase::HandleArrowReadyToRecycle);
		Arrow->Destroy();
	}

	AllSpawnedArrows.Reset();
	AvailableArrows.Reset();
}

void ABowBase::HandleArrowReadyToRecycle(AArrowBase* Arrow)
{
	if (!IsValid(Arrow) || !AllSpawnedArrows.Contains(Arrow))
	{
		return;
	}

	if (Arrow == PreparedArrow)
	{
		PreparedArrow = nullptr;
	}

	if (Arrow == LastFiredArrow)
	{
		LastFiredArrow = nullptr;
	}

	AvailableArrows.AddUnique(Arrow);
}