#include "BowBase.h"

#include "ArrowDataAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"


ABowBase::ABowBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BowMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BowMesh"));
	SetRootComponent(BowMesh);
}

void ABowBase::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float TargetAlpha = bDrawVisualsActive ? 1.0f : 0.0f;
	const float InterpSpeed = bDrawVisualsActive ? DrawInterpSpeed : ReleaseInterpSpeed;

	DrawAlpha = FMath::FInterpTo(DrawAlpha, TargetAlpha, DeltaTime, InterpSpeed);

	const USkeletalMeshComponent* Mesh = GetWielderMesh();

	if (bDrawVisualsActive &&
		IsValid(Mesh) &&
		!DrawHandSocketName.IsNone() &&
		Mesh->DoesSocketExist(DrawHandSocketName))
	{
		StringTargetLocation = Mesh->GetSocketLocation(DrawHandSocketName);
	}

	if (!bDrawVisualsActive && FMath::IsNearlyZero(DrawAlpha, 0.001f))
	{
		DrawAlpha = 0.0f;
		SetActorTickEnabled(false);
	}
}

void ABowBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndDrawVisuals();
	DestroyArrowPool();

	Super::EndPlay(EndPlayReason);
}

void ABowBase::BeginDrawVisuals()
{
	if (bDrawVisualsActive)
	{
		return;
	}

	bDrawVisualsActive = true;
	SetActorTickEnabled(true);
}

void ABowBase::EndDrawVisuals()
{
	bDrawVisualsActive = false;
}

bool ABowBase::PrepareArrows(UArrowDataAsset* ArrowData, const int32 ArrowCount)
{
	if (HasPreparedArrows() ||
		!IsValid(ArrowData) ||
		!ArrowData->ArrowClass ||
		ArrowCount <= 0)
	{
		return false;
	}

	PreparedArrows.Reserve(ArrowCount);

	for (int32 Index = 0; Index < ArrowCount; ++Index)
	{
		AArrowBase* Arrow = AcquireAvailableArrow(ArrowData->ArrowClass);

		if (!IsValid(Arrow) || !Arrow->ActivateFromPool(ArrowData))
		{
			if (IsValid(Arrow))
			{
				Arrow->ReturnToPool();
			}

			DiscardPreparedArrows();
			return false;
		}

		PreparedArrows.Add(Arrow);
	}

	return PreparedArrows.Num() == ArrowCount;
}

bool ABowBase::AttachPreparedArrowToWielder(const int32 ArrowIndex, const FName SocketName)
{
	USkeletalMeshComponent* Mesh = GetWielderMesh();
	if (!PreparedArrows.IsValidIndex(ArrowIndex) ||
		!IsValid(PreparedArrows[ArrowIndex]) ||
		!IsValid(Mesh) ||
		SocketName.IsNone() ||
		!Mesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	return PreparedArrows[ArrowIndex]->AttachToComponent(
		Mesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName
	);
}
void ABowBase::OnHolstered_Implementation()
{
	Super::OnHolstered_Implementation();

	EndDrawVisuals();
	DiscardPreparedArrows();
}
bool ABowBase::AttachPreparedArrowToBow(const int32 ArrowIndex, const FName SocketName)
{
	if (!PreparedArrows.IsValidIndex(ArrowIndex) ||
		!IsValid(PreparedArrows[ArrowIndex]) ||
		!IsValid(BowMesh) ||
		SocketName.IsNone() ||
		!BowMesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	return PreparedArrows[ArrowIndex]->AttachToComponent(
		BowMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName
	);
}

bool ABowBase::ReleasePreparedArrows(const TArray<FVector>& Directions, const FArrowShotParams& ShotParams)
{
	if (PreparedArrows.IsEmpty() ||
		Directions.Num() != PreparedArrows.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < PreparedArrows.Num(); ++Index)
	{
		if (!IsValid(PreparedArrows[Index]) ||
			Directions[Index].GetSafeNormal().IsNearlyZero())
		{
			return false;
		}
	}

	TArray<TObjectPtr<AArrowBase>> ArrowsToFire = PreparedArrows;
	PreparedArrows.Reset();
	ReleasedArrows.Reset();

	
	bool bReleasedAnyArrow = false;

	for (int32 Index = 0; Index < ArrowsToFire.Num(); ++Index)
	{
		AArrowBase* Arrow = ArrowsToFire[Index];

		if (!IsValid(Arrow))
		{
			continue;
		}

		Arrow->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		if (!Arrow->Fire(Directions[Index].GetSafeNormal(), ShotParams))
		{
			Arrow->ReturnToPool();
			continue;
		}

		ReleasedArrows.Add(Arrow);
		OnArrowFired.Broadcast(Arrow, Arrow->GetFiredStrength());
		bReleasedAnyArrow = true;
	}

	return bReleasedAnyArrow;
}

void ABowBase::DiscardPreparedArrows()
{
	for (AArrowBase* Arrow : PreparedArrows)
	{
		if (IsValid(Arrow))
		{
			Arrow->ReturnToPool();
		}
	}

	PreparedArrows.Reset();
}
AArrowBase* ABowBase::GetPreparedArrow(const int32 ArrowIndex) const
{
	return PreparedArrows.IsValidIndex(ArrowIndex)
		? PreparedArrows[ArrowIndex].Get()
		: nullptr;
}

AArrowBase* ABowBase::GetReleasedArrow(const int32 ArrowIndex) const
{
	return ReleasedArrows.IsValidIndex(ArrowIndex)
		? ReleasedArrows[ArrowIndex].Get()
		: nullptr;
}
AArrowBase* ABowBase::CreateArrow(const TSubclassOf<AArrowBase> ArrowClass)
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

AArrowBase* ABowBase::AcquireAvailableArrow(const TSubclassOf<AArrowBase> ArrowClass)
{
	for (int32 Index = AvailableArrows.Num() - 1; Index >= 0; --Index)
	{
		AArrowBase* Candidate = AvailableArrows[Index];

		if (!IsValid(Candidate))
		{
			AvailableArrows.RemoveAtSwap(Index);
			continue;
		}

		if (Candidate->IsA(ArrowClass))
		{
			AvailableArrows.RemoveAtSwap(Index);
			return Candidate;
		}
	}

	return CreateArrow(ArrowClass);
}

void ABowBase::DestroyArrowPool()
{
	PreparedArrows.Reset();
	ReleasedArrows.Reset();

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

	PreparedArrows.Remove(Arrow);
	ReleasedArrows.Remove(Arrow);

	AvailableArrows.AddUnique(Arrow);
}