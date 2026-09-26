#include "RangedAttackAbility.h"

#include "ArrowBase.h"
#include "ArrowDataAsset.h"
#include "BowBase.h"
#include "GameFramework/Character.h"
#include "TargetingComponent.h"
#include "BowDataAsset.h"

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
	ACharacter* Character = GetOwningCharacter();

	Bow = GetEquippedWeaponAs<ABowBase>();

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;

	if (!IsValid(Bow))
	{
		RequestCancelAbility();
		return;
	}
	Bow->HandleFeedbackPoint(EBowFeedbackPoint::AbilityStart,BowData);

	Bow->DiscardPreparedArrows();

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

		Bow->HandleFeedbackPoint(
			EBowFeedbackPoint::AbilityEnd,
			BowData
		);
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

	if (!Bow->PrepareArrows(ArrowData, ProjectileHandSocketNames.Num()))
	{
		return false;
	}

	for (int32 Index = 0; Index < ProjectileHandSocketNames.Num(); ++Index)
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

	Bow->HandleFeedbackPoint(
		EBowFeedbackPoint::SpawnArrow,
		BowData
	);

	OnProjectilePrepared();
	return true;
}

bool URangedAttackAbility::NockProjectile_Implementation()
{
	if (!IsValid(Bow) ||
		!bProjectilePrepared ||
		bProjectileReleased ||
		ProjectileBowSocketNames.IsEmpty() ||
		ProjectileBowSocketNames.Num() != Bow->GetPreparedArrowCount())
	{
		return false;
	}

	for (int32 Index = 0; Index < ProjectileBowSocketNames.Num(); ++Index)
	{
		if (!Bow->AttachPreparedArrowToBow(Index, ProjectileBowSocketNames[Index]))
		{
			return false;
		}
	}

	bProjectileNocked = true;

	Bow->BeginDrawVisuals();

	Bow->HandleFeedbackPoint(
		EBowFeedbackPoint::NockArrow,
		BowData
	);

	for (int32 Index = 0; Index < Bow->GetPreparedArrowCount(); ++Index)
	{
		if (AArrowBase* PreparedArrow = Bow->GetPreparedArrow(Index))
		{
			PreparedArrow->PlayStartFeedback();
		}
	}

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

	const float Strength = FMath::Clamp(ResolveProjectileStrength(), 0.0f, 1.0f);

	if (!Bow->ReleasePreparedArrows(Directions, Strength, bHasTarget))
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

	Bow->HandleFeedbackPoint(
		EBowFeedbackPoint::ReleaseArrow,
		BowData
	);

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = true;

	OnProjectileReleased(Strength);
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
