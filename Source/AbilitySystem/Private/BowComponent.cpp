#include "BowComponent.h"

#include "ArrowBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

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
	Bow->DiscardPreparedArrow();
	Bow->EndDrawVisuals();
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

bool UBowComponent::PrepareArrow(const FArrowStats& ArrowStats)
{
	return IsValid(Bow) && Bow->PrepareArrow(ArrowStats);
}

bool UBowComponent::AttachPreparedArrowToWielder(const FName SocketName, const FTransform& RelativeOffset)
{
	return IsValid(Bow) && Bow->AttachPreparedArrowToWielder(SocketName, RelativeOffset);
}

bool UBowComponent::AttachPreparedArrowToBow(const FName SocketName, const FTransform& RelativeOffset)
{
	return IsValid(Bow) && Bow->AttachPreparedArrowToBow(SocketName, RelativeOffset);
}

bool UBowComponent::ReleasePreparedArrow(const FVector& Direction, const float Strength)
{
	return IsValid(Bow) && Bow->ReleasePreparedArrow(Direction, Strength);
}

void UBowComponent::DiscardPreparedArrow()
{
	if (IsValid(Bow))
	{
		Bow->DiscardPreparedArrow();
	}
}

AArrowBase* UBowComponent::GetPreparedArrow() const
{
	return IsValid(Bow) ? Bow->GetPreparedArrow() : nullptr;
}

AArrowBase* UBowComponent::GetLastFiredArrow() const
{
	return IsValid(Bow) ? Bow->GetLastFiredArrow() : nullptr;
}

bool UBowComponent::HasPreparedArrow() const
{
	return IsValid(Bow) && Bow->HasPreparedArrow();
}

void UBowComponent::HandleBowArrowFired(AArrowBase* Arrow, const float ShotStrength)
{
	OnArrowFired.Broadcast(Arrow, ShotStrength);
}