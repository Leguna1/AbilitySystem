#include "RangedAttackAbility.h"

#include "ArrowBase.h"
#include "ArrowDataAsset.h"
#include "BowBase.h"
#include "GameFramework/Character.h"
#include "TargetingComponent.h"
#include "AbilitySystem/Public/ImpactGroupSubsystem.h"

bool URangedAttackAbility::CanActivateAbility_Implementation() const
{
	if (!Super::CanActivateAbility_Implementation())
	{
		return false;
	}

	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character) ||
		!IsValid(ArrowData) ||
		!ArrowData->ArrowClass ||
		!IsValid(ArrowData->ArrowMesh) ||
		ProjectileHandSocketNames.IsEmpty() ||
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
	Bow = GetEquippedWeaponAs<ABowBase>();

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;

	if (!IsValid(Bow))
	{
		RequestCancelAbility();
		return;
	}

	Bow->DiscardPreparedArrows();
	
	// Fixed for this execution, like ranks: a booster ending mid-volley doesn't change it.
	ResolvedDamageMultiplier = FMath::Max(
		GetModifiedFloat(AbilitySystemTags::Stat_Damage, GetRankedFloat(DamageMultiplierByRank)),
		0.0f
	);

	ResolvedProjectileCount = ResolveProjectileCount();

	ResolvedPierceCount = FMath::Max(GetModifiedInt(AbilitySystemTags::Stat_PierceCount, BasePierceCount), 0);
	ResolvedPierceDamageFactor = FMath::Max(GetModifiedFloat(AbilitySystemTags::Stat_PierceDamage, PierceDamageFactor), 0.0f);
	
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

	OnRangedAttackFinished(
		EndReason,
		bHadPreparedProjectile,
		bReleasedProjectile
	);

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
	const int32 SocketCount = ProjectileHandSocketNames.Num();

	const int32 BaseCount = ProjectileCountByRank.Values.IsEmpty()
		? SocketCount
		: GetRankedInt(ProjectileCountByRank);

	return FMath::Clamp(
		GetModifiedInt(AbilitySystemTags::Stat_ProjectileCount, BaseCount),
		1,
		SocketCount
	);
}
float URangedAttackAbility::ResolveProjectileDamageMultiplier_Implementation() const
{
	return ResolvedDamageMultiplier;
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
		!IsValid(ArrowData) ||
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

	if (!Bow->PrepareArrows(ArrowData, ProjectileCount))
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
	if (!IsValid(Bow) ||
		!bProjectilePrepared ||
		bProjectileReleased ||
		!Bow->HasPreparedArrows())
	{
		return false;
	}

	const bool bHasTarget =
		ShouldUseCurrentTarget() &&
		IsValid(GetTargetingComponent()) &&
		GetTargetingComponent()->HasTarget();

	TArray<FVector> Directions;
	Directions.Reserve(Bow->GetPreparedArrowCount());

	for (int32 Index = 0; Index < Bow->GetPreparedArrowCount(); ++Index)
	{
		FVector Direction = FVector::ZeroVector;

		if (bHasTarget)
		{
			const AArrowBase* PreparedArrow = Bow->GetPreparedArrow(Index);

			if (!IsValid(PreparedArrow))
			{
				return false;
			}

			Direction = (
				GetTargetingComponent()->GetCurrentTargetAimLocation() -
				PreparedArrow->GetActorLocation()
			).GetSafeNormal();
		}
		else
		{
			Direction = ResolveProjectileDirectionForIndex(Index).GetSafeNormal();
		}

		if (Direction.IsNearlyZero())
		{
			return false;
		}

		Directions.Add(Direction);
	}

	FArrowShotParams ShotParams;
	ShotParams.Strength = FMath::Clamp(ResolveProjectileStrength(), 0.0f, 1.0f);
	ShotParams.bTargetedShot = bHasTarget;
	ShotParams.DamageMultiplier = FMath::Max(ResolveProjectileDamageMultiplier(), 0.0f);
	ShotParams.SourceAbilityTags = GetAbilityTags();
	ResolveOnHitStatuses(ShotParams.Statuses);
	ShotParams.PierceCount = ResolvedPierceCount;
	ShotParams.PierceDamageFactor = ResolvedPierceDamageFactor;

	// Open before release so every arrow can join; seal right after so the
	// group closes once the last arrow resolves.
	UImpactGroupSubsystem* ImpactGroups = bGroupProjectileImpacts
		? UImpactGroupSubsystem::Get(this)
		: nullptr;

	if (IsValid(ImpactGroups))
	{
		ShotParams.ImpactGroup = ImpactGroups->OpenGroup(ProjectileImpactGroup);
	}

	const bool bReleased = Bow->ReleasePreparedArrows(Directions, ShotParams);

	if (IsValid(ImpactGroups))
	{
		ImpactGroups->SealGroup(ShotParams.ImpactGroup);
	}

	if (!bReleased)
	{
		return false;
	}
	
	ReleasedProjectiles.Reset();

	for (int32 Index = 0; Index < Bow->GetReleasedArrowCount(); ++Index)
	{
		if (AArrowBase* Arrow = Bow->GetReleasedArrow(Index))
		{
			ReleasedProjectiles.Add(Arrow);
		}
	}
	Bow->EndDrawVisuals();

	

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = true;

	OnProjectileReleased(ShotParams.Strength);
	return true;
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

AArrowBase* URangedAttackAbility::GetReleasedProjectile(const int32 ProjectileIndex) const
{
	return ReleasedProjectiles.IsValidIndex(ProjectileIndex)
		? ReleasedProjectiles[ProjectileIndex].Get()
		: nullptr;
}

bool URangedAttackAbility::HandleProjectileAnimationEvent(const FGameplayTag EventTag)
{
	if (PrepareProjectileEventTag.IsValid() &&
		EventTag.MatchesTagExact(PrepareProjectileEventTag))
	{
		return PrepareProjectile();
	}

	if (NockProjectileEventTag.IsValid() &&
		EventTag.MatchesTagExact(NockProjectileEventTag))
	{
		return NockProjectile();
	}

	if (ReleaseProjectileEventTag.IsValid() &&
		EventTag.MatchesTagExact(ReleaseProjectileEventTag))
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
