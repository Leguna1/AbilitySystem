#include "AnimNotifyState_WeaponEffect.h"

#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "WeaponManagerComponent.h"

void UAnimNotifyState_WeaponEffect::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	USceneComponent* AttachTo = FindAttachComponent(MeshComp);

	if (!IsValid(Effect) || !IsValid(AttachTo))
	{
		return;
	}

	UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(
		Effect,
		AttachTo,
		SocketName,
		LocationOffset,
		RotationOffset,
		EAttachLocation::SnapToTarget,
		true
	);

	if (IsValid(Spawned))
	{
		Spawned->ComponentTags.Add(GetEffectTag());
	}
}

void UAnimNotifyState_WeaponEffect::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	USceneComponent* AttachTo = FindAttachComponent(MeshComp);

	if (!IsValid(AttachTo))
	{
		return;
	}

	const FName EffectTag = GetEffectTag();

	TArray<USceneComponent*> Children;
	AttachTo->GetChildrenComponents(false, Children);

	for (USceneComponent* Child : Children)
	{
		UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Child);

		if (!IsValid(Niagara) || !Niagara->ComponentHasTag(EffectTag))
		{
			continue;
		}

		// Untag so a later window's end doesn't touch this one while it finishes.
		Niagara->ComponentTags.Remove(EffectTag);

		if (bLetParticlesFinish)
		{
			Niagara->Deactivate();
		}
		else
		{
			Niagara->DestroyComponent();
		}
	}
}

FString UAnimNotifyState_WeaponEffect::GetNotifyName_Implementation() const
{
	return IsValid(Effect)
		? FString::Printf(TEXT("Weapon Effect: %s"), *Effect->GetName())
		: TEXT("Weapon Effect");
}

USceneComponent* UAnimNotifyState_WeaponEffect::FindAttachComponent(USkeletalMeshComponent* MeshComp) const
{
	if (!IsValid(MeshComp))
	{
		return nullptr;
	}

	const AActor* Owner = MeshComp->GetOwner();
	const UWeaponManagerComponent* WeaponManager = IsValid(Owner)
		? Owner->FindComponentByClass<UWeaponManagerComponent>()
		: nullptr;

	AWeaponBase* Weapon = IsValid(WeaponManager) ? WeaponManager->GetEquippedWeapon() : nullptr;

	if (!IsValid(Weapon))
	{
		// Editor preview has no weapon manager: attach to the previewed mesh so timing is visible.
		const UWorld* World = MeshComp->GetWorld();
		return IsValid(World) && !World->IsGameWorld() ? MeshComp : nullptr;
	}

	if (!SocketName.IsNone())
	{
		TInlineComponentArray<UMeshComponent*> WeaponMeshes(Weapon);

		for (UMeshComponent* WeaponMesh : WeaponMeshes)
		{
			if (WeaponMesh->DoesSocketExist(SocketName))
			{
				return WeaponMesh;
			}
		}
	}

	return Weapon->GetRootComponent();
}

FName UAnimNotifyState_WeaponEffect::GetEffectTag() const
{
	return FName(*FString::Printf(TEXT("WeaponEffect_%u"), GetUniqueID()));
}