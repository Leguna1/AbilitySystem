#include "WeaponBase.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "WeaponDataAsset.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWeaponBase::InitializeWeapon(ACharacter* InWielder, UWeaponDataAsset* InWeaponData)
{
	Wielder = InWielder;
	WielderMesh = IsValid(Wielder) ? Wielder->GetMesh() : nullptr;
	WeaponData = InWeaponData;
	bDrawn = false;
}

void AWeaponBase::HandleDrawn()
{
	if (bDrawn)
	{
		return;
	}

	// Without data, placement is left to the caller (transitional path for the old components).
	if (IsValid(WeaponData))
	{
		AttachToWielderSocket(WeaponData->EquipSocketName);
	}

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	bDrawn = true;
	OnDrawn();
}

void AWeaponBase::HandleHolstered()
{
	const bool bWasDrawn = bDrawn;
	bDrawn = false;

	// Subclass cleanup runs while still attached to the hand.
	if (bWasDrawn)
	{
		OnHolstered();
	}

	ApplyHolsteredPlacement();
}

bool AWeaponBase::IsWielderOwned(const AActor* Actor) const
{
	for (const AActor* Current = Actor; IsValid(Current); Current = Current->GetOwner())
	{
		if (Current == Wielder || Current == this)
		{
			return true;
		}
	}

	return false;
}

void AWeaponBase::OnDrawn_Implementation()
{
}

void AWeaponBase::OnHolstered_Implementation()
{
}

bool AWeaponBase::AttachToWielderSocket(const FName SocketName)
{
	if (!IsValid(WielderMesh))
	{
		return false;
	}

	if (!SocketName.IsNone() && !WielderMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: socket %s not found on wielder mesh."),
			*GetName(), *SocketName.ToString());
		return false;
	}

	return AttachToComponent(WielderMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, SocketName);
}

void AWeaponBase::ApplyHolsteredPlacement()
{
	const FName HolsterSocket = IsValid(WeaponData) ? WeaponData->HolsterSocketName : NAME_None;
	const bool bOnHolster = !HolsterSocket.IsNone() && AttachToWielderSocket(HolsterSocket);

	SetActorHiddenInGame(!bOnHolster);
	SetActorEnableCollision(false);
}
UMeshComponent* AWeaponBase::GetWeaponMesh() const
{
	return FindComponentByClass<UMeshComponent>();
}

bool AWeaponBase::GetLaunchTransform(const int32 ProjectileIndex, FTransform& OutTransform) const
{
	const UMeshComponent* WeaponMesh = GetWeaponMesh();

	if (LaunchSocketNames.IsEmpty() || !IsValid(WeaponMesh))
	{
		return false;
	}

	const FName SocketName = LaunchSocketNames[FMath::Abs(ProjectileIndex) % LaunchSocketNames.Num()];

	if (!WeaponMesh->DoesSocketExist(SocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: launch socket %s not found on the weapon mesh."), *GetName(), *SocketName.ToString());
		return false;
	}

	OutTransform = WeaponMesh->GetSocketTransform(SocketName);
	return true;
}
