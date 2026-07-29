#include "RangedAttackAbility.h"

#include "ArrowBase.h"
#include "ArrowDataAsset.h"
#include "BowComponent.h"
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

	const UBowComponent* CharacterBowComponent = Character->FindComponentByClass<UBowComponent>();

	return IsValid(CharacterBowComponent) &&
		CharacterBowComponent->HasEquippedBow();
}
void URangedAttackAbility::ActivateAbility_Implementation()
{
	ACharacter* Character = GetOwningCharacter();

	BowComponent = IsValid(Character)
		? Character->FindComponentByClass<UBowComponent>()
		: nullptr;
	

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;

	if (!IsValid(BowComponent) || !BowComponent->HasEquippedBow())
	{
		RequestCancelAbility();
		return;
	}
	BowComponent->HandleFeedbackPoint(EBowFeedbackPoint::AbilityStart,BowData);

	BowComponent->DiscardPreparedArrows();

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
	ReleasedProjectiles.Reset();
	
	const bool bHadPreparedProjectile =
		bProjectilePrepared ||
		(IsValid(BowComponent) && BowComponent->HasPreparedArrows());

	const bool bReleasedProjectile = bProjectileReleased;

	if (IsValid(BowComponent))
	{
		BowComponent->EndDrawVisuals();

		BowComponent->HandleFeedbackPoint(
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

	BowComponent = nullptr;

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;
}

bool URangedAttackAbility::HasPreparedProjectile() const
{
	return bProjectilePrepared &&
		IsValid(BowComponent) &&
		BowComponent->HasPreparedArrows();
}

void URangedAttackAbility::ResetProjectileCycle()
{
	if (IsValid(BowComponent) && BowComponent->HasPreparedArrows())
	{
		BowComponent->DiscardPreparedArrows();
	}

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;
}

void URangedAttackAbility::DiscardPreparedProjectile()
{
	if (IsValid(BowComponent))
	{
		BowComponent->EndDrawVisuals();

		if (BowComponent->HasPreparedArrows())
		{
			BowComponent->DiscardPreparedArrows();
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
	if (!IsValid(BowComponent) ||
		!IsValid(ArrowData) ||
		ProjectileHandSocketNames.IsEmpty() ||
		ProjectileHandSocketNames.Num() != ProjectileBowSocketNames.Num())
	{
		return false;
	}

	if (BowComponent->HasPreparedArrows())
	{
		BowComponent->DiscardPreparedArrows();
	}

	if (!BowComponent->PrepareArrows(ArrowData, ProjectileHandSocketNames.Num()))
	{
		return false;
	}

	for (int32 Index = 0; Index < ProjectileHandSocketNames.Num(); ++Index)
	{
		if (!BowComponent->AttachPreparedArrowToWielder(Index, ProjectileHandSocketNames[Index]))
		{
			BowComponent->DiscardPreparedArrows();
			return false;
		}
	}

	bProjectilePrepared = true;
	bProjectileNocked = false;
	bProjectileReleased = false;

	BowComponent->HandleFeedbackPoint(
		EBowFeedbackPoint::SpawnArrow,
		BowData
	);

	OnProjectilePrepared();
	return true;
}

bool URangedAttackAbility::NockProjectile_Implementation()
{
	if (!IsValid(BowComponent) ||
		!bProjectilePrepared ||
		bProjectileReleased ||
		ProjectileBowSocketNames.IsEmpty() ||
		ProjectileBowSocketNames.Num() != BowComponent->GetPreparedArrowCount())
	{
		return false;
	}

	for (int32 Index = 0; Index < ProjectileBowSocketNames.Num(); ++Index)
	{
		if (!BowComponent->AttachPreparedArrowToBow(Index, ProjectileBowSocketNames[Index]))
		{
			return false;
		}
	}

	bProjectileNocked = true;

	BowComponent->BeginDrawVisuals();

	BowComponent->HandleFeedbackPoint(
		EBowFeedbackPoint::NockArrow,
		BowData
	);

	for (int32 Index = 0; Index < BowComponent->GetPreparedArrowCount(); ++Index)
	{
		if (AArrowBase* PreparedArrow = BowComponent->GetPreparedArrow(Index))
		{
			PreparedArrow->PlayStartFeedback();
		}
	}

	OnProjectileNocked();
	return true;
}

bool URangedAttackAbility::ReleaseProjectile_Implementation()
{
	if (!IsValid(BowComponent) ||
		!bProjectilePrepared ||
		bProjectileReleased ||
		!BowComponent->HasPreparedArrows())
	{
		return false;
	}

	const bool bHasTarget =
		ShouldUseCurrentTarget() &&
		IsValid(GetTargetingComponent()) &&
		GetTargetingComponent()->HasTarget();

	TArray<FVector> Directions;
	Directions.Reserve(BowComponent->GetPreparedArrowCount());

	for (int32 Index = 0; Index < BowComponent->GetPreparedArrowCount(); ++Index)
	{
		FVector Direction = FVector::ZeroVector;

		if (bHasTarget)
		{
			const AArrowBase* PreparedArrow = BowComponent->GetPreparedArrow(Index);

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

	if (!BowComponent->ReleasePreparedArrows(Directions, Strength, bHasTarget))
	{
		return false;
	}
	
	ReleasedProjectiles.Reset();

	for (int32 Index = 0; Index < BowComponent->GetReleasedArrowCount(); ++Index)
	{
		if (AArrowBase* Arrow = BowComponent->GetReleasedArrow(Index))
		{
			ReleasedProjectiles.Add(Arrow);
		}
	}
	BowComponent->EndDrawVisuals();

	BowComponent->HandleFeedbackPoint(
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
