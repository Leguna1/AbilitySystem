#include "RangedAttackAbility.h"

#include "AbilitySystemSettings.h"
#include "BowBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "ProjectileBase.h"
#include "ProjectileDataAsset.h"
#include "ProjectileFragment.h"
#include "ProjectilePoolSubsystem.h"
#include "WeaponBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	/** Upper bound for Launch Point releases, so a runaway modifier can't spawn hundreds. */
	constexpr int32 MaxLaunchPointProjectiles = 32;
}

bool URangedAttackAbility::CanActivateAbility_Implementation() const
{
	if (!Super::CanActivateAbility_Implementation())
	{
		return false;
	}

	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character) ||
		!IsValid(ProjectileData) ||
		!ProjectileData->ProjectileClass)
	{
		return false;
	}

	if (LaunchSource == EProjectileLaunchSource::LaunchPoint)
	{
		return true;
	}

	if (ProjectileHandSocketNames.IsEmpty() ||
		ProjectileHandSocketNames.Num() != ProjectileBowSocketNames.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < ProjectileHandSocketNames.Num(); ++Index)
	{
		if (ProjectileHandSocketNames[Index].IsNone() ||
			ProjectileBowSocketNames[Index].IsNone())
		{
			return false;
		}
	}

	const ABowBase* EquippedBow = GetEquippedWeaponAs<ABowBase>();

	return IsValid(EquippedBow) && EquippedBow->IsDrawn();
}

void URangedAttackAbility::ActivateAbility_Implementation()
{
	Bow = LaunchSource == EProjectileLaunchSource::Bow
		? GetEquippedWeaponAs<ABowBase>()
		: nullptr;

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;
	ReleasedProjectiles.Reset();

	if (LaunchSource == EProjectileLaunchSource::Bow)
	{
		if (!IsValid(Bow))
		{
			RequestCancelAbility();
			return;
		}

		Bow->DiscardPreparedArrows();
	}

	// Fixed for this execution, like ranks: a booster ending mid-volley doesn't change it.
	ResolvedDamage = FMath::Max(
		GetModifiedFloat(AbilitySystemTags::Stat_Damage, GetRankedFloat(DamageByRank)),
		0.0f);

	ResolvedProjectileCount = ResolveProjectileCount();

	Super::ActivateAbility_Implementation();
}

void URangedAttackAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (!EventTag.IsValid())
	{
		return;
	}

	if (!HandleProjectileAnimationEvent(EventTag))
	{
		RequestCancelAbility();
	}
}

void URangedAttackAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	const bool bHadPreparedProjectile =
		bProjectilePrepared ||
		(IsValid(Bow) && Bow->HasPreparedArrows());

	const bool bReleasedProjectile = bProjectileReleased;

	if (IsValid(Bow))
	{
		Bow->EndDrawVisuals();
	}

	DiscardPreparedProjectile();

	Super::OnAbilityEnded_Implementation(EndReason);

	OnRangedAttackFinished(EndReason, bHadPreparedProjectile, bReleasedProjectile);

	Bow = nullptr;

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;
	ReleasedProjectiles.Reset();
}

bool URangedAttackAbility::HasPreparedProjectile() const
{
	return bProjectilePrepared &&
		IsValid(Bow) &&
		Bow->HasPreparedArrows();
}

int32 URangedAttackAbility::GetProjectileCount() const
{
	return ResolvedProjectileCount;
}

int32 URangedAttackAbility::ResolveProjectileCount() const
{
	if (LaunchSource == EProjectileLaunchSource::LaunchPoint)
	{
		const int32 BaseCount = ProjectileCountByRank.Values.IsEmpty() ? 1 : GetRankedInt(ProjectileCountByRank);

		return FMath::Clamp(
			GetModifiedInt(AbilitySystemTags::Stat_ProjectileCount, BaseCount),
			1,
			MaxLaunchPointProjectiles);
	}

	const int32 SocketCount = ProjectileHandSocketNames.Num();

	const int32 BaseCount = ProjectileCountByRank.Values.IsEmpty()
		? SocketCount
		: GetRankedInt(ProjectileCountByRank);

	return FMath::Clamp(
		GetModifiedInt(AbilitySystemTags::Stat_ProjectileCount, BaseCount),
		1,
		SocketCount);
}

float URangedAttackAbility::ResolveProjectileDamage_Implementation() const
{
	return ResolvedDamage;
}

void URangedAttackAbility::ResetProjectileCycle()
{
	if (IsValid(Bow) && Bow->HasPreparedArrows())
	{
		Bow->DiscardPreparedArrows();
	}

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;
}

void URangedAttackAbility::DiscardPreparedProjectile()
{
	if (IsValid(Bow))
	{
		Bow->EndDrawVisuals();

		if (Bow->HasPreparedArrows())
		{
			Bow->DiscardPreparedArrows();
		}
	}

	bProjectilePrepared = false;
	bProjectileNocked = false;
}

FVector URangedAttackAbility::ResolveProjectileDirectionForIndex_Implementation(const int32 ProjectileIndex) const
{
	return ResolveProjectileDirection();
}

bool URangedAttackAbility::PrepareProjectile_Implementation()
{
	if (!IsValid(Bow) ||
		!IsValid(ProjectileData) ||
		ProjectileHandSocketNames.IsEmpty() ||
		ProjectileHandSocketNames.Num() != ProjectileBowSocketNames.Num())
	{
		return false;
	}

	if (Bow->HasPreparedArrows())
	{
		Bow->DiscardPreparedArrows();
	}

	const int32 ProjectileCount = GetProjectileCount();

	if (!Bow->PrepareArrows(ProjectileData, ProjectileCount))
	{
		return false;
	}

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		if (!Bow->AttachPreparedArrowToWielder(Index, ProjectileHandSocketNames[Index]))
		{
			Bow->DiscardPreparedArrows();
			return false;
		}
	}

	bProjectilePrepared = true;
	bProjectileNocked = false;
	bProjectileReleased = false;

	OnProjectilePrepared();
	return true;
}

bool URangedAttackAbility::NockProjectile_Implementation()
{
	if (!IsValid(Bow) ||
		!bProjectilePrepared ||
		bProjectileReleased ||
		ProjectileBowSocketNames.IsEmpty() ||
		Bow->GetPreparedArrowCount() > ProjectileBowSocketNames.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < Bow->GetPreparedArrowCount(); ++Index)
	{
		if (!Bow->AttachPreparedArrowToBow(Index, ProjectileBowSocketNames[Index]))
		{
			return false;
		}
	}

	bProjectileNocked = true;

	Bow->BeginDrawVisuals();

	OnProjectileNocked();
	return true;
}

bool URangedAttackAbility::ReleaseProjectile_Implementation()
{
	if (bProjectileReleased || !IsValid(ProjectileData))
	{
		return false;
	}

	const bool bFromBow = LaunchSource == EProjectileLaunchSource::Bow;

	if (bFromBow && (!IsValid(Bow) || !bProjectilePrepared || !Bow->HasPreparedArrows()))
	{
		return false;
	}

	// Where each projectile starts: the nocked arrows, or the launch points.
	const int32 ProjectileCount = bFromBow ? Bow->GetPreparedArrowCount() : GetProjectileCount();

	TArray<FTransform> StartTransforms;
	StartTransforms.Reserve(ProjectileCount);

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		if (bFromBow)
		{
			const AProjectileBase* PreparedArrow = Bow->GetPreparedArrow(Index);

			if (!IsValid(PreparedArrow))
			{
				return false;
			}

			StartTransforms.Add(PreparedArrow->GetActorTransform());
		}
		else
		{
			StartTransforms.Add(ResolveLaunchTransform(Index));
		}
	}

	// The player's target or the AI's target, the same way for both.
	const bool bHasTarget = ShouldUseCurrentTarget() && HasAbilityTarget();
	const FVector TargetAimLocation = bHasTarget ? GetAbilityTargetAimLocation() : FVector::ZeroVector;

	TArray<FVector> Directions;
	Directions.Reserve(ProjectileCount);

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		const FVector Direction = bHasTarget
			? (TargetAimLocation - StartTransforms[Index].GetLocation()).GetSafeNormal()
			: ResolveProjectileDirectionForIndex(Index).GetSafeNormal();

		if (Direction.IsNearlyZero())
		{
			return false;
		}

		Directions.Add(Direction);
	}

	FArrowShotParams ShotParams;
	ShotParams.Strength = FMath::Clamp(ResolveProjectileStrength(), 0.0f, 1.0f);
	ShotParams.bTargetedShot = bHasTarget;
	ShotParams.Damage = FMath::Max(ResolveProjectileDamage(), 0.0f);
	ShotParams.Hit = BuildHitSpec();
	ShotParams.FeedbackSet = ProjectileFeedback;
	ShotParams.FeedbackStrength = ShotParams.Strength;

	// Pierce from stats alone (e.g. a passive); a Pierce feature supplies base values instead.
	ShotParams.PierceCount = FMath::Max(GetModifiedInt(AbilitySystemTags::Stat_PierceCount, 0), 0);
	ShotParams.PierceDamageFactor = FMath::Max(
		GetModifiedFloat(AbilitySystemTags::Stat_PierceDamage, GetDefault<UAbilitySystemSettings>()->DefaultPierceDamageFactor),
		0.0f);

	// Features get the final say over the shot.
	ForEachFragment<UProjectileFragment>([this, &ShotParams](const UProjectileFragment& Fragment)
	{
		Fragment.ModifyShotParams(*this, ShotParams);
	});

	// Every projectile joins this use's group, so a volley lands as one sound.
	ShotParams.ImpactGroup = GetImpactGroup();

	ReleasedProjectiles.Reset();
	bool bReleased = false;

	if (bFromBow)
	{
		bReleased = Bow->ReleasePreparedArrows(Directions, ShotParams);

		for (int32 Index = 0; bReleased && Index < Bow->GetReleasedArrowCount(); ++Index)
		{
			if (AProjectileBase* Arrow = Bow->GetReleasedArrow(Index))
			{
				ReleasedProjectiles.Add(Arrow);
			}
		}

		Bow->EndDrawVisuals();
	}
	else
	{
		bReleased = LaunchFromPoints(StartTransforms, Directions, ShotParams);
	}

	if (!bReleased)
	{
		return false;
	}

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = true;

	OnProjectileReleased(ShotParams.Strength);
	NotifyFragmentsProjectilesReleased();

	return true;
}

bool URangedAttackAbility::LaunchFromPoints(const TArray<FTransform>& StartTransforms, const TArray<FVector>& Directions, const FArrowShotParams& ShotParams)
{
	UProjectilePoolSubsystem* Pool = UProjectilePoolSubsystem::Get(this);
	ACharacter* OwnerCharacter = GetOwningCharacter();

	if (!IsValid(Pool) || !IsValid(OwnerCharacter) || StartTransforms.Num() != Directions.Num())
	{
		return false;
	}

	// Owned by the weapon when there is one (like the bow's arrows), else by the character.
	AActor* LaunchOwner = IsValid(GetEquippedWeapon())
		? static_cast<AActor*>(GetEquippedWeapon())
		: static_cast<AActor*>(OwnerCharacter);

	for (int32 Index = 0; Index < StartTransforms.Num(); ++Index)
	{
		AProjectileBase* Projectile = Pool->AcquireProjectile(ProjectileData, LaunchOwner, OwnerCharacter);

		if (!IsValid(Projectile))
		{
			continue;
		}

		Projectile->SetActorLocationAndRotation(
			StartTransforms[Index].GetLocation(),
			Directions[Index].Rotation(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);

		if (!Projectile->Fire(Directions[Index], ShotParams))
		{
			Projectile->ReturnToPool();
			continue;
		}

		ReleasedProjectiles.Add(Projectile);
	}

	return !ReleasedProjectiles.IsEmpty();
}

FTransform URangedAttackAbility::ResolveLaunchTransform(const int32 ProjectileIndex) const
{
	// 1. The equipped weapon's launch sockets (staff tip, blade).
	if (const AWeaponBase* EquippedWeapon = GetEquippedWeapon())
	{
		FTransform WeaponLaunchTransform;

		if (EquippedWeapon->GetLaunchTransform(ProjectileIndex, WeaponLaunchTransform))
		{
			return WeaponLaunchTransform;
		}
	}

	const ACharacter* OwnerCharacter = GetOwningCharacter();

	if (!IsValid(OwnerCharacter))
	{
		return FTransform::Identity;
	}

	// 2. Sockets on the character (a hand).
	const USkeletalMeshComponent* CharacterMesh = OwnerCharacter->GetMesh();

	if (!CharacterLaunchSocketNames.IsEmpty() && IsValid(CharacterMesh))
	{
		const FName SocketName = CharacterLaunchSocketNames[FMath::Abs(ProjectileIndex) % CharacterLaunchSocketNames.Num()];

		if (CharacterMesh->DoesSocketExist(SocketName))
		{
			return CharacterMesh->GetSocketTransform(SocketName);
		}
	}

	// 3. Just in front of the eyes.
	FVector EyeLocation;
	FRotator EyeRotation;
	OwnerCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	return FTransform(EyeRotation, EyeLocation + OwnerCharacter->GetActorForwardVector() * 50.0f);
}

FVector URangedAttackAbility::ResolveProjectileDirection_Implementation() const
{
	const ACharacter* Character = GetOwningCharacter();

	return IsValid(Character)
		? Character->GetActorForwardVector().GetSafeNormal()
		: FVector::ZeroVector;
}

float URangedAttackAbility::ResolveProjectileStrength_Implementation() const
{
	return DefaultProjectileStrength;
}

bool URangedAttackAbility::ShouldUseCurrentTarget_Implementation() const
{
	return true;
}

void URangedAttackAbility::OnProjectilePrepared_Implementation()
{
}

void URangedAttackAbility::OnProjectileNocked_Implementation()
{
}

void URangedAttackAbility::OnProjectileReleased_Implementation(float Strength)
{
}

void URangedAttackAbility::OnRangedAttackFinished_Implementation(EAbilityEndReason EndReason, bool bHadPreparedProjectile, bool bReleasedProjectile)
{
}

AProjectileBase* URangedAttackAbility::GetReleasedProjectile(const int32 ProjectileIndex) const
{
	return ReleasedProjectiles.IsValidIndex(ProjectileIndex)
		? ReleasedProjectiles[ProjectileIndex].Get()
		: nullptr;
}

bool URangedAttackAbility::HandleProjectileAnimationEvent(const FGameplayTag EventTag)
{
	// Prepare and nock only exist for bows.
	if (LaunchSource == EProjectileLaunchSource::Bow)
	{
		if (PrepareProjectileEventTag.IsValid() && EventTag.MatchesTagExact(PrepareProjectileEventTag))
		{
			return PrepareProjectile();
		}

		if (NockProjectileEventTag.IsValid() && EventTag.MatchesTagExact(NockProjectileEventTag))
		{
			return NockProjectile();
		}
	}

	if (ReleaseProjectileEventTag.IsValid() && EventTag.MatchesTagExact(ReleaseProjectileEventTag))
	{
		return ReleaseProjectile();
	}

	return true;
}

FGameplayTag URangedAttackAbility::GetCostEventTag() const
{
	const FGameplayTag Configured = Super::GetCostEventTag();
	return Configured.IsValid() ? Configured : ReleaseProjectileEventTag;
}

void URangedAttackAbility::NotifyFragmentsProjectilesReleased()
{
	TArray<AProjectileBase*> Released;

	for (int32 Index = 0; Index < GetReleasedProjectileCount(); ++Index)
	{
		if (AProjectileBase* Projectile = GetReleasedProjectile(Index))
		{
			Released.Add(Projectile);
		}
	}

	if (Released.IsEmpty())
	{
		return;
	}

	ForEachFragment<UProjectileFragment>([this, &Released](const UProjectileFragment& Fragment)
	{
		Fragment.OnProjectilesReleased(*this, Released);
	});
}

#if WITH_EDITOR
EDataValidationResult URangedAttackAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!IsValid(ProjectileData))
	{
		Context.AddError(NSLOCTEXT("RangedAttack", "NoProjectile", "Projectile Data is empty, so nothing can be fired."));
		Result = EDataValidationResult::Invalid;
	}

	if (!ReleaseProjectileEventTag.IsValid())
	{
		Context.AddWarning(NSLOCTEXT("RangedAttack", "NoRelease",
			"Release Projectile Event Tag is empty: nothing fires unless this ability releases from code. Usually: set it, and add an Ability Event notify with that tag to the montage."));
	}

	if (LaunchSource == EProjectileLaunchSource::Bow &&
		(ProjectileHandSocketNames.IsEmpty() || ProjectileHandSocketNames.Num() != ProjectileBowSocketNames.Num()))
	{
		Context.AddError(NSLOCTEXT("RangedAttack", "BowSockets",
			"Bow launch needs one Hand Socket and one Bow Socket per arrow, in matching pairs."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
