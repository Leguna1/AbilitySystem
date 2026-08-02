#include "MontageAbility.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"

void UMontageAbility::ActivateAbility_Implementation()
{
	UAnimMontage* Montage = SelectAbilityMontage();

	if (!PlayAbilityMontage(Montage, MontagePlayRate))
	{
		RequestCancelAbility();
	}
}

void UMontageAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	const float BlendOutTime = EndReason == EAbilityEndReason::EarlyCancelled
		? EarlyCancellationBlendOutTime
		: EndBlendOutTime;

	StopAbilityMontage(BlendOutTime);
}

UAnimMontage* UMontageAbility::SelectAbilityMontage_Implementation() const
{
	return AbilityMontage;
}

bool UMontageAbility::PlayAbilityMontage(UAnimMontage* Montage, const float PlayRate)
{
	UAnimInstance* AnimInstance = GetAnimInstance();

	if (!IsValid(AnimInstance) || !IsValid(Montage))
	{
		return false;
	}

	CloseTransition();

	ActiveMontage = nullptr;

	const float Duration = AnimInstance->Montage_Play(
		Montage,
		PlayRate,
		EMontagePlayReturnType::MontageLength,
		0.0f,
		true
	);

	if (Duration <= 0.0f)
	{
		return false;
	}

	ActiveMontage = Montage;

	// Install the root-motion distance warp target before the warp window hits.
	ApplyRootMotionDistanceWarp();

	FOnMontageBlendingOutStarted BlendingOutDelegate;
	BlendingOutDelegate.BindUObject(this, &UMontageAbility::HandleMontageBlendingOut);
	AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, Montage);

	FOnMontageEnded EndedDelegate;
	EndedDelegate.BindUObject(this, &UMontageAbility::HandleMontageEnded);
	AnimInstance->Montage_SetEndDelegate(EndedDelegate, Montage);

	OnAbilityMontageStarted(Montage);
	return true;
}

void UMontageAbility::StopAbilityMontage(const float BlendOutTime)
{
	UAnimInstance* AnimInstance = GetAnimInstance();
	UAnimMontage* MontageToStop = ActiveMontage;

	ActiveMontage = nullptr;

	if (IsValid(AnimInstance) &&
		IsValid(MontageToStop) &&
		AnimInstance->Montage_IsActive(MontageToStop))
	{
		AnimInstance->Montage_Stop(BlendOutTime, MontageToStop);
	}
}

bool UMontageAbility::IsAbilityMontagePlaying() const
{
	const UAnimInstance* AnimInstance = GetAnimInstance();

	return IsValid(AnimInstance) &&
		IsValid(ActiveMontage) &&
		AnimInstance->Montage_IsPlaying(ActiveMontage);
}

void UMontageAbility::OnAbilityMontageStarted_Implementation(UAnimMontage* Montage)
{
}

void UMontageAbility::OnAbilityMontageBlendingOut_Implementation(UAnimMontage* Montage, bool bInterrupted)
{
}

void UMontageAbility::OnAbilityMontageEnded_Implementation(UAnimMontage* Montage, const bool bInterrupted)
{
	if (!bEndAbilityWhenMontageEnds)
	{
		return;
	}

	if (bInterrupted)
	{
		RequestCancelAbility();
		return;
	}

	RequestEndAbility();
}

void UMontageAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (OpenTransitionEventTag.IsValid() &&
		EventTag.MatchesTagExact(OpenTransitionEventTag))
	{
		OpenTransition();
	}

	if (CloseEarlyCancellationEventTag.IsValid() &&
		EventTag.MatchesTagExact(CloseEarlyCancellationEventTag))
	{
		CloseEarlyCancellation();
	}
}

UAnimInstance* UMontageAbility::GetAnimInstance() const
{
	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character) || !IsValid(Character->GetMesh()))
	{
		return nullptr;
	}

	return Character->GetMesh()->GetAnimInstance();
}

void UMontageAbility::HandleMontageBlendingOut(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != ActiveMontage)
	{
		return;
	}

	OnAbilityMontageBlendingOut(Montage, bInterrupted);
}

void UMontageAbility::HandleMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != ActiveMontage)
	{
		return;
	}

	ActiveMontage = nullptr;
	OnAbilityMontageEnded(Montage, bInterrupted);
}
FVector UMontageAbility::GetRootMotionWarpDirection_Implementation() const
{
	const ACharacter* Character = GetOwningCharacter();
	if (!IsValid(Character))
	{
		return FVector::ZeroVector;
	}

	return Character->GetActorForwardVector().GetSafeNormal2D();
}

void UMontageAbility::ApplyRootMotionDistanceWarp() const
{
	UMotionWarpingComponent* Warping = GetMotionWarpingComponent();
	if (!IsValid(Warping) || RootMotionWarpName.IsNone())
	{
		return;
	}

	const ACharacter* Character = GetOwningCharacter();
	if (!IsValid(Character))
	{
		return;
	}

	// Not overriding: make sure no stale distance target lingers from a prior play.
	if (!bOverrideRootMotionDistance)
	{
		Warping->RemoveWarpTarget(RootMotionWarpName);
		return;
	}

	const FVector Direction = GetRootMotionWarpDirection().GetSafeNormal2D();

	// Zero direction (or zero distance) collapses the target onto the character,
	// which warps the montage to translate in place.
	const FVector StartLocation = Character->GetActorLocation();
	const FVector TargetLocation = Direction.IsNearlyZero()
		? StartLocation
		: StartLocation + Direction * RootMotionDistance;

	// Keep the character's current facing for the warp target rotation; distance
	// warping only needs the translation goal.
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(
		RootMotionWarpName,
		TargetLocation,
		Character->GetActorRotation()
	);
}
