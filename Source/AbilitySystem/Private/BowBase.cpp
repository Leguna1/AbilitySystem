#include "BowBase.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "FeedbackComponent.h"
#include "FeedbackTags.h"
#include "ProjectileBase.h"
#include "ProjectileDataAsset.h"
#include "ProjectilePoolSubsystem.h"

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

	const USkeletalMeshComponent* WielderSkeletalMesh = GetWielderMesh();

	if (bDrawVisualsActive &&
		IsValid(WielderSkeletalMesh) &&
		!DrawHandSocketName.IsNone() &&
		WielderSkeletalMesh->DoesSocketExist(DrawHandSocketName))
	{
		StringTargetLocation = WielderSkeletalMesh->GetSocketLocation(DrawHandSocketName);
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
	DiscardPreparedArrows();

	// Arrows already in flight belong to the pool and keep flying; just stop listening to them.
	for (AProjectileBase* Arrow : ReleasedArrows)
	{
		if (IsValid(Arrow))
		{
			Arrow->OnReadyToRecycle.RemoveDynamic(this, &ABowBase::HandleArrowReadyToRecycle);
		}
	}

	ReleasedArrows.Reset();

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

	FeedbackComponent->StartLoop(FeedbackTags::Cue_Weapon_Charge, BowMesh);
}

void ABowBase::EndDrawVisuals()
{
	if (bDrawVisualsActive)
	{
		FeedbackComponent->ClearLoop(FeedbackTags::Cue_Weapon_Charge);
	}

	bDrawVisualsActive = false;
}

bool ABowBase::PrepareArrows(UProjectileDataAsset* ArrowData, const int32 ArrowCount)
{
	UProjectilePoolSubsystem* Pool = UProjectilePoolSubsystem::Get(this);

	if (HasPreparedArrows() ||
		!IsValid(Pool) ||
		!IsValid(ArrowData) ||
		ArrowCount <= 0)
	{
		return false;
	}

	PreparedArrows.Reserve(ArrowCount);

	for (int32 Index = 0; Index < ArrowCount; ++Index)
	{
		// Owned by the bow, so the arrow never hits the bow or (through the instigator) the archer.
		AProjectileBase* Arrow = Pool->AcquireProjectile(ArrowData, this, GetInstigator());

		if (!IsValid(Arrow))
		{
			DiscardPreparedArrows();
			return false;
		}

		TrackArrow(Arrow);
		PreparedArrows.Add(Arrow);
	}

	return PreparedArrows.Num() == ArrowCount;
}

bool ABowBase::AttachPreparedArrowToWielder(const int32 ArrowIndex, const FName SocketName)
{
	USkeletalMeshComponent* WielderSkeletalMesh = GetWielderMesh();

	if (!PreparedArrows.IsValidIndex(ArrowIndex) ||
		!IsValid(PreparedArrows[ArrowIndex]) ||
		!IsValid(WielderSkeletalMesh) ||
		SocketName.IsNone() ||
		!WielderSkeletalMesh->DoesSocketExist(SocketName))
	{
		return false;
	}

	return PreparedArrows[ArrowIndex]->AttachToComponent(
		WielderSkeletalMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		SocketName);
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

	AProjectileBase* Arrow = PreparedArrows[ArrowIndex];

	if (!Arrow->AttachToComponent(BowMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName))
	{
		return false;
	}

	// Glows on the string until release; Fire ends it and starts the flight loop.
	USceneComponent* LoopTarget = Arrow->GetRootComponent();
	Arrow->GetFeedback()->StartLoop(FeedbackTags::Cue_Projectile_Nocked, LoopTarget);
	return true;
}

bool ABowBase::ReleasePreparedArrows(const TArray<FVector>& Directions, const FArrowShotParams& ShotParams)
{
	if (PreparedArrows.IsEmpty() || Directions.Num() != PreparedArrows.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < PreparedArrows.Num(); ++Index)
	{
		if (!IsValid(PreparedArrows[Index]) || Directions[Index].GetSafeNormal().IsNearlyZero())
		{
			return false;
		}
	}

	TArray<TObjectPtr<AProjectileBase>> ArrowsToFire = PreparedArrows;
	PreparedArrows.Reset();

	for (AProjectileBase* PreviousArrow : ReleasedArrows)
	{
		UntrackArrow(PreviousArrow);
	}

	ReleasedArrows.Reset();

	bool bReleasedAnyArrow = false;

	for (int32 Index = 0; Index < ArrowsToFire.Num(); ++Index)
	{
		AProjectileBase* Arrow = ArrowsToFire[Index];

		if (!IsValid(Arrow))
		{
			continue;
		}

		Arrow->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		if (!Arrow->Fire(Directions[Index].GetSafeNormal(), ShotParams))
		{
			UntrackArrow(Arrow);
			Arrow->ReturnToPool();
			continue;
		}

		ReleasedArrows.Add(Arrow);
		OnArrowFired.Broadcast(Arrow, Arrow->GetFiredStrength());
		bReleasedAnyArrow = true;
	}

	if (bReleasedAnyArrow)
	{
		// One twang per release, however many arrows left the string.
		FeedbackComponent->PlayCueAt(FeedbackTags::Cue_Weapon_Attack, BowMesh->GetComponentLocation(), Directions[0].Rotation());
	}

	return bReleasedAnyArrow;
}

void ABowBase::DiscardPreparedArrows()
{
	TArray<TObjectPtr<AProjectileBase>> ArrowsToDiscard = PreparedArrows;
	PreparedArrows.Reset();

	for (AProjectileBase* Arrow : ArrowsToDiscard)
	{
		if (IsValid(Arrow))
		{
			UntrackArrow(Arrow);
			Arrow->ReturnToPool();
		}
	}
}

AProjectileBase* ABowBase::GetPreparedArrow(const int32 ArrowIndex) const
{
	return PreparedArrows.IsValidIndex(ArrowIndex) ? PreparedArrows[ArrowIndex].Get() : nullptr;
}

AProjectileBase* ABowBase::GetReleasedArrow(const int32 ArrowIndex) const
{
	return ReleasedArrows.IsValidIndex(ArrowIndex) ? ReleasedArrows[ArrowIndex].Get() : nullptr;
}

void ABowBase::TrackArrow(AProjectileBase* Arrow)
{
	if (IsValid(Arrow))
	{
		Arrow->OnReadyToRecycle.AddUniqueDynamic(this, &ABowBase::HandleArrowReadyToRecycle);
	}
}

void ABowBase::UntrackArrow(AProjectileBase* Arrow)
{
	if (IsValid(Arrow))
	{
		Arrow->OnReadyToRecycle.RemoveDynamic(this, &ABowBase::HandleArrowReadyToRecycle);
	}
}

void ABowBase::HandleArrowReadyToRecycle(AProjectileBase* Arrow)
{
	// The pool may hand this arrow to someone else next, so the bow forgets it now.
	UntrackArrow(Arrow);
	PreparedArrows.Remove(Arrow);
	ReleasedArrows.Remove(Arrow);
}
