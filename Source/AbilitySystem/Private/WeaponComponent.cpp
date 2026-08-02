// WeaponComponent.cpp
#include "WeaponComponent.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningCharacter = Cast<ACharacter>(GetOwner());
	if (!IsValid(OwningCharacter))
	{
		UE_LOG(LogTemp, Warning, TEXT("[WeaponComponent] Owner is not a Character."));
	}
}

void UWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnequipWeapon();
	Super::EndPlay(EndPlayReason);
}

bool UWeaponComponent::EquipWeapon()
{
	if (IsValid(Weapon))
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(OwningCharacter) || !WeaponClass)
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

	Weapon = World->SpawnActor<ASwordBase>(WeaponClass, FTransform::Identity, SpawnParameters);
	if (!IsValid(Weapon))
	{
		return false;
	}

	if (!Weapon->AttachToComponent(CharacterMesh, FAttachmentTransformRules::SnapToTargetIncludingScale, WeaponSocketName))
	{
		Weapon->Destroy();
		Weapon = nullptr;
		return false;
	}

	return true;
}

void UWeaponComponent::UnequipWeapon()
{
	if (!IsValid(Weapon))
	{
		return;
	}

	Weapon->Destroy();
	Weapon = nullptr;
}

void UWeaponComponent::BeginWeaponHitDetection()
{
	if (IsValid(Weapon))
	{
		Weapon->BeginHitDetection();
	}
}

void UWeaponComponent::EndWeaponHitDetection()
{
	if (IsValid(Weapon))
	{
		Weapon->EndHitDetection();
	}
}