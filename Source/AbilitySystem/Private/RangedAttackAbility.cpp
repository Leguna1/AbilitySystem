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

	if (!IsValid(Character))
	{
		return false;
	}

	const UBowComponent* CharacterBowComponent = Character->FindComponentByClass<UBowComponent>();

	return IsValid(CharacterBowComponent) &&
		CharacterBowComponent->HasEquippedBow() &&
		IsValid(ArrowData) &&
		ArrowData->ArrowClass &&
		IsValid(ArrowData->ArrowMesh) &&
		!ProjectileHandSocketName.IsNone();
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

	BowComponent->DiscardPreparedArrow();

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
		(IsValid(BowComponent) && BowComponent->HasPreparedArrow());

	const bool bReleasedProjectile = bProjectileReleased;

	if (IsValid(BowComponent))
	{
		BowComponent->EndDrawVisuals();

		BowComponent->HandleFeedbackPoint(EBowFeedbackPoint::AbilityEnd,BowData);
	}
	
	DiscardPreparedProjectile();

	Super::OnAbilityEnded_Implementation(EndReason);

	OnRangedAttackFinished(EndReason, bHadPreparedProjectile, bReleasedProjectile);

	BowComponent = nullptr;
	
	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;
}

bool URangedAttackAbility::HasPreparedProjectile() const
{
	return bProjectilePrepared &&
		IsValid(BowComponent) &&
		BowComponent->HasPreparedArrow();
}

void URangedAttackAbility::ResetProjectileCycle()
{
	if (IsValid(BowComponent) && BowComponent->HasPreparedArrow())
	{
		BowComponent->DiscardPreparedArrow();
	}

	bProjectilePrepared = false;
	bProjectileNocked = false;
	bProjectileReleased = false;
}

void URangedAttackAbility::DiscardPreparedProjectile()
{
	if (IsValid(BowComponent))
	{
		

		if (BowComponent->HasPreparedArrow())
		{
			BowComponent->DiscardPreparedArrow();
		}
	}

	bProjectilePrepared = false;
	bProjectileNocked = false;
}

bool URangedAttackAbility::PrepareProjectile_Implementation()
{
	if (!IsValid(BowComponent) ||
		!IsValid(ArrowData) ||
		ProjectileHandSocketName.IsNone())
	{
		return false;
	}

	if (BowComponent->HasPreparedArrow())
	{
		BowComponent->DiscardPreparedArrow();
	}

	if (!BowComponent->PrepareArrow(ArrowData))
	{
		return false;
	}

	if (!BowComponent->AttachPreparedArrowToWielder(
		ProjectileHandSocketName,
		ProjectileHandOffset))
	{
		BowComponent->DiscardPreparedArrow();
		return false;
	}

	bProjectilePrepared = true;
	bProjectileNocked = false;
	bProjectileReleased = false;
	BowComponent->BeginDrawVisuals();
	
	BowComponent->HandleFeedbackPoint(EBowFeedbackPoint::SpawnArrow,BowData);
	OnProjectilePrepared();
	return true;
}

bool URangedAttackAbility::NockProjectile_Implementation()
{
	if (!IsValid(BowComponent) ||
		!bProjectilePrepared ||
		bProjectileReleased ||
		ProjectileBowSocketName.IsNone() ||
		!BowComponent->HasPreparedArrow())
	{
		return false;
	}

	if (!BowComponent->AttachPreparedArrowToBow(ProjectileBowSocketName, ProjectileBowOffset))
	{
		return false;
	}

	bProjectileNocked = true;
	
	BowComponent->HandleFeedbackPoint(EBowFeedbackPoint::NockArrow,BowData);
	
	if (AArrowBase* PreparedArrow = BowComponent->GetPreparedArrow())
	{
		PreparedArrow->PlayStartFeedback();
	}
	
	

	OnProjectileNocked();
	return true;
}

bool URangedAttackAbility::ReleaseProjectile_Implementation()
{
	if (!IsValid(BowComponent) ||
		!bProjectilePrepared ||
		bProjectileReleased ||
		!BowComponent->HasPreparedArrow())
	{
		return false;
	}

	AArrowBase* PreparedArrow = BowComponent->GetPreparedArrow();

	if (!IsValid(PreparedArrow))
	{
		return false;
	}

	const bool bHasTarget =
	ShouldUseCurrentTarget() &&
	IsValid(GetTargetingComponent()) &&
	GetTargetingComponent()->HasTarget();

	FVector Direction = FVector::ZeroVector;

	if (bHasTarget)
	{
		const FVector ProjectileLocation = PreparedArrow->GetActorLocation();
		const FVector TargetLocation = GetTargetingComponent()->GetCurrentTargetAimLocation();

		Direction = (TargetLocation - ProjectileLocation).GetSafeNormal();
	}
	else
	{
		Direction = ResolveProjectileDirection().GetSafeNormal();
	}

	if (Direction.IsNearlyZero())
	{
		return false;
	}

	const float Strength = FMath::Clamp(ResolveProjectileStrength(), 0.0f, 1.0f);

	if (!BowComponent->ReleasePreparedArrow(Direction, Strength, bHasTarget))
	{
		return false;
	}
	
	BowComponent->EndDrawVisuals();
	BowComponent->HandleFeedbackPoint(EBowFeedbackPoint::ReleaseArrow,BowData);

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