#include "WeaponBase.h"

#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "FeedbackComponent.h"
#include "FeedbackSubsystem.h"
#include "FeedbackTags.h"
#include "GameFramework/Character.h"
#include "ImpactGroupSubsystem.h"
#include "WeaponDataAsset.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;

	FeedbackComponent = CreateDefaultSubobject<UFeedbackComponent>(TEXT("Feedback"));
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

	FeedbackComponent->PlayCue(FeedbackTags::Cue_Weapon_Draw);
	FeedbackComponent->StartLoop(FeedbackTags::Cue_Weapon_Aura, GetWeaponMesh());

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

		// Anything still running on the weapon (aura, trail, charge) ends with it.
		FeedbackComponent->StopAllLoops(false);
		FeedbackComponent->PlayCue(FeedbackTags::Cue_Weapon_Holster);
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

void AWeaponBase::PlayHitFeedback(AActor* HitActor, const FHitResult& Hit, const FVector StrikeLocation, const bool bHitTarget, const FImpactGroupHandle& Group)
{
	UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this);

	if (!IsValid(ImpactGroups))
	{
		return;
	}

	FImpactReport Report;
	Report.Group = Group;
	Report.Result = bHitTarget ? EImpactResult::Hit : EImpactResult::Miss;
	Report.Location = StrikeLocation;

	// Facing from the wielder into the target: blood and sparks fly the way the blow went.
	const FVector From = IsValid(Wielder) ? Wielder->GetActorLocation() : GetActorLocation();
	Report.Rotation = (StrikeLocation - From).GetSafeNormal().Rotation();

	Report.FeedbackSet = FeedbackComponent->GetActiveSet();
	Report.FallbackSet = FeedbackComponent->GetFallbackSet();
	Report.HitCue = FeedbackTags::Cue_Weapon_Hit;
	Report.MissCue = FeedbackTags::Cue_Weapon_Miss;
	Report.Instigator = Wielder;
	Report.Target = HitActor;
	Report.Hit = Hit;

	// The target may already be dying; only read it while it is still valid.
	if (IsValid(Hit.GetComponent()))
	{
		Report.HitComponent = Hit.GetComponent();
	}
	else if (IsValid(HitActor))
	{
		Report.HitComponent = Cast<UPrimitiveComponent>(HitActor->GetRootComponent());
	}

	ImpactGroups->PlayImpact(Report);
}

FVector AWeaponBase::ResolveStrikeLocation(const AActor* HitActor, const FHitResult& Hit) const
{
	const UMeshComponent* WeaponMesh = GetWeaponMesh();
	const FVector WeaponPoint = IsValid(WeaponMesh) ? WeaponMesh->Bounds.Origin : GetActorLocation();

	return UFeedbackSubsystem::ResolveStrikeLocation(HitActor, Hit, WeaponPoint);
}
