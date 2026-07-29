#include "BowComponent.h"

#include "ArrowBase.h"
#include "ArrowDataAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "BowDataAsset.h"

UBowComponent::UBowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBowComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningCharacter = Cast<ACharacter>(GetOwner());

	if (!IsValid(OwningCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("UBowComponent requires an ACharacter owner."));
		return;
	}

	if (bEquipBowOnBeginPlay)
	{
		EquipBow();
	}
}

void UBowComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnequipBow();

	Super::EndPlay(EndPlayReason);
}

bool UBowComponent::EquipBow()
{
	if (IsValid(Bow))
	{
		return true;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World) || !IsValid(OwningCharacter) || !BowClass)
	{
		return false;
	}

	USkeletalMeshComponent* CharacterMesh = OwningCharacter->GetMesh();

	if (!IsValid(CharacterMesh))
	{
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwningCharacter;
	SpawnParameters.Instigator = OwningCharacter;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Bow = World->SpawnActor<ABowBase>(BowClass, FTransform::Identity, SpawnParameters);

	if (!IsValid(Bow))
	{
		return false;
	}

	if (!Bow->AttachToComponent(
		CharacterMesh,
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		BowSocketName))
	{
		Bow->Destroy();
		Bow = nullptr;
		return false;
	}

	Bow->SetWielderMesh(CharacterMesh);
	Bow->OnArrowFired.AddDynamic(this, &UBowComponent::HandleBowArrowFired);

	return true;
}

void UBowComponent::UnequipBow()
{
	if (!IsValid(Bow))
	{
		return;
	}

	Bow->OnArrowFired.RemoveDynamic(this, &UBowComponent::HandleBowArrowFired);
	Bow->EndDrawVisuals();
	Bow->ClearAllFeedback();
	Bow->DiscardPreparedArrows();
	Bow->Destroy();

	Bow = nullptr;
}

void UBowComponent::BeginDrawVisuals()
{
	if (IsValid(Bow))
	{
		Bow->BeginDrawVisuals();
	}
}

void UBowComponent::EndDrawVisuals()
{
	if (IsValid(Bow))
	{
		Bow->EndDrawVisuals();
	}
}

void UBowComponent::HandleFeedbackPoint(const EBowFeedbackPoint FeedbackPoint, UBowDataAsset* BowData)
{
	if (IsValid(Bow))
	{
		Bow->HandleFeedbackPoint(FeedbackPoint, BowData);
	}
}

void UBowComponent::ClearAllFeedback()
{
	if (IsValid(Bow))
	{
		Bow->ClearAllFeedback();
	}
}

bool UBowComponent::PrepareArrows(UArrowDataAsset* ArrowData, const int32 ArrowCount)
{
	return IsValid(Bow) && Bow->PrepareArrows(ArrowData, ArrowCount);
}

bool UBowComponent::AttachPreparedArrowToWielder(const int32 ArrowIndex, const FName SocketName)
{
	return IsValid(Bow) && Bow->AttachPreparedArrowToWielder(ArrowIndex, SocketName);
}

bool UBowComponent::AttachPreparedArrowToBow(const int32 ArrowIndex, const FName SocketName)
{
	return IsValid(Bow) && Bow->AttachPreparedArrowToBow(ArrowIndex, SocketName);
}

bool UBowComponent::ReleasePreparedArrows(const TArray<FVector>& Directions, const float Strength, const bool bTargetedShot)
{
	return IsValid(Bow) && Bow->ReleasePreparedArrows(Directions, Strength, bTargetedShot);
}

void UBowComponent::DiscardPreparedArrows()
{
	if (IsValid(Bow))
	{
		Bow->DiscardPreparedArrows();
	}
}

AArrowBase* UBowComponent::GetPreparedArrow(const int32 ArrowIndex) const
{
	return IsValid(Bow)
		? Bow->GetPreparedArrow(ArrowIndex)
		: nullptr;
}

int32 UBowComponent::GetPreparedArrowCount() const
{
	return IsValid(Bow)
		? Bow->GetPreparedArrowCount()
		: 0;
}

bool UBowComponent::HasPreparedArrows() const
{
	return IsValid(Bow) && Bow->HasPreparedArrows();
}
void UBowComponent::HandleBowArrowFired(AArrowBase* Arrow, const float ShotStrength)
{
	OnArrowFired.Broadcast(Arrow, ShotStrength);
}
int32 UBowComponent::GetReleasedArrowCount() const
{
	return IsValid(Bow)
		? Bow->GetReleasedArrowCount()
		: 0;
}

AArrowBase* UBowComponent::GetReleasedArrow(const int32 ArrowIndex) const
{
	return IsValid(Bow)
		? Bow->GetReleasedArrow(ArrowIndex)
		: nullptr;
}