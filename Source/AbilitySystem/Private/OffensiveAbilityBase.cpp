#include "OffensiveAbilityBase.h"

#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "TargetingComponent.h"

void UOffensiveAbilityBase::ActivateAbility_Implementation()
{
	ConfigureTargetFacingWarp();

	Super::ActivateAbility_Implementation();

	if (GetAbilityStatus() == EAbilityStatus::Active)
	{
		OnAttackStarted();
	}
}

void UOffensiveAbilityBase::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	Super::OnAbilityEnded_Implementation(EndReason);

	if (bRemoveTargetFacingWarpWhenFinished &&
		!TargetFacingWarpName.IsNone() &&
		IsValid(GetMotionWarpingComponent()))
	{
		GetMotionWarpingComponent()->RemoveWarpTarget(TargetFacingWarpName);
	}

	OnAttackFinished(EndReason);
}

bool UOffensiveAbilityBase::ConfigureTargetFacingWarp()
{
	if (!bUseTargetFacingWarp ||
		TargetFacingWarpName.IsNone() ||
		!IsValid(GetMotionWarpingComponent()) ||
		!IsValid(GetTargetingComponent()) ||
		!GetTargetingComponent()->HasTarget())
	{
		return false;
	}

	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character))
	{
		return false;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	const FVector TargetLocation = GetTargetingComponent()->GetCurrentTargetAimLocation();

	FVector FlatDirection = TargetLocation - CharacterLocation;
	FlatDirection.Z = 0.0f;

	if (FlatDirection.IsNearlyZero())
	{
		return false;
	}

	
	GetMotionWarpingComponent()->AddOrUpdateWarpTargetFromLocationAndRotation(
		TargetFacingWarpName,
		CharacterLocation,
		FlatDirection.Rotation()
	);

	return true;
}

void UOffensiveAbilityBase::OnAttackStarted_Implementation()
{
}

void UOffensiveAbilityBase::OnAttackFinished_Implementation(EAbilityEndReason EndReason)
{
}