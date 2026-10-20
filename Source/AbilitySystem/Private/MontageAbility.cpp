#include "MontageAbility.h"

#include "AbilityComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "AbilitySystemTags.h"
#include "AttributeComponent.h"
#include "CombatantComponent.h"

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
	UnbindAttackSpeedUpdates();

	const float BlendOutTime = bEndedByMovement
	? MovementCancelBlendOutTime
	: EndReason == EAbilityEndReason::EarlyCancelled
		? EarlyCancellationBlendOutTime
		: EndBlendOutTime;

	StopAbilityMontage(BlendOutTime);
}

UAnimMontage* UMontageAbility::SelectAbilityMontage_Implementation() const
{
	for (const FMontageVariant& Variant : MontageVariants)
	{
		if (IsValid(Variant.Montage) && IsMontageVariantMet(Variant))
		{
			return Variant.Montage;
		}
	}

	return AbilityMontage;
}

bool UMontageAbility::IsMontageVariantMet(const FMontageVariant& Variant) const
{
	const ACharacter* OwnerCharacter = GetOwningCharacter();
	const AActor* VariantTarget = GetAbilityTarget();

	if (Variant.Condition == EMontageVariantCondition::NoTarget)
	{
		return !IsValid(VariantTarget);
	}

	if (Variant.Condition == EMontageVariantCondition::SelfMovingFasterThan)
	{
		return IsValid(OwnerCharacter) && OwnerCharacter->GetVelocity().Size2D() > Variant.Threshold;
	}

	if (!IsValid(OwnerCharacter) || !IsValid(VariantTarget))
	{
		return false;
	}

	const float TargetSpeed = VariantTarget->GetVelocity().Size2D();
	const float Gap = FVector::Dist2D(OwnerCharacter->GetActorLocation(), VariantTarget->GetActorLocation()) -
		OwnerCharacter->GetSimpleCollisionRadius() - VariantTarget->GetSimpleCollisionRadius();

	switch (Variant.Condition)
	{
	case EMontageVariantCondition::TargetMovingFasterThan:
		return TargetSpeed > Variant.Threshold;

	case EMontageVariantCondition::TargetSlowerThan:
		return TargetSpeed <= Variant.Threshold;

	case EMontageVariantCondition::TargetFartherThan:
		return Gap > Variant.Threshold;

	case EMontageVariantCondition::TargetCloserThan:
		return Gap < Variant.Threshold;

	default:
		return false;
	}
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
	BaseMontagePlayRate = PlayRate;

	const float Duration = AnimInstance->Montage_Play(
		Montage,
		PlayRate * GetAttackSpeed(),
		EMontagePlayReturnType::MontageLength,
		0.0f,
		true
	);

	if (Duration <= 0.0f)
	{
		return false;
	}

	ActiveMontage = Montage;
	BindAttackSpeedUpdates();

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

float UMontageAbility::GetAttackSpeed() const
{
	if (!bAffectedByAttackSpeed)
	{
		return 1.0f;
	}

	const UAbilityComponent* Component = GetAbilityComponent();
	const UAttributeComponent* Attributes = IsValid(Component) ? Component->GetAttributeComponent() : nullptr;

	// This ability's own view: the character's attack speed plus any scoped to its tags. No attribute = normal speed.
	const float AttackSpeed = IsValid(Attributes)
		? Attributes->GetAttributeValueForAbility(AbilitySystemTags::Stat_AttackSpeed, GetAbilityTags(), 1.0f)
		: 1.0f;

	return FMath::Max(AttackSpeed, 0.1f);
}

void UMontageAbility::BindAttackSpeedUpdates()
{
	if (!bAffectedByAttackSpeed || !bUpdateAttackSpeedDuringMontage || AttackSpeedSource.IsValid())
	{
		return;
	}

	// Effects live on the combatant; any change there may change this ability's attack speed.
	const UAbilityComponent* Component = GetAbilityComponent();
	UCombatantComponent* Combatant = IsValid(Component) ? Component->GetCombatantComponent() : nullptr;

	if (IsValid(Combatant))
	{
		Combatant->OnEffectsChanged.AddUniqueDynamic(this, &UMontageAbility::HandleOwnerEffectsChanged);
		AttackSpeedSource = Combatant;
	}
}

void UMontageAbility::UnbindAttackSpeedUpdates()
{
	if (UCombatantComponent* Combatant = AttackSpeedSource.Get())
	{
		Combatant->OnEffectsChanged.RemoveDynamic(this, &UMontageAbility::HandleOwnerEffectsChanged);
	}

	AttackSpeedSource.Reset();
}

void UMontageAbility::HandleOwnerEffectsChanged()
{
	UAnimInstance* AnimInstance = GetAnimInstance();

	if (!IsValid(AnimInstance) || !IsValid(ActiveMontage) || !AnimInstance->Montage_IsPlaying(ActiveMontage))
	{
		return;
	}

	const float NewPlayRate = BaseMontagePlayRate * GetAttackSpeed();

	if (!FMath::IsNearlyEqual(AnimInstance->Montage_GetPlayRate(ActiveMontage), NewPlayRate))
	{
		AnimInstance->Montage_SetPlayRate(ActiveMontage, NewPlayRate);
	}
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

bool UMontageAbility::AcceptsAnimationEvent(const UAnimSequenceBase* SourceAnimation) const
{
	if (!IsValid(SourceAnimation))
	{
		return true;
	}

	const UAnimMontage* Montage = ActiveMontage.Get();

	if (!IsValid(Montage))
	{
		return false;
	}

	if (SourceAnimation == Montage)
	{
		return true;
	}

	// Notifies authored inside a sequence report that sequence, not the montage.
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
		{
			if (Segment.GetAnimReference() == SourceAnimation)
			{
				return true;
			}
		}
	}

	return false;
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
	if (MovementCancelEventTag.IsValid() &&
	EventTag.MatchesTagExact(MovementCancelEventTag))
	{
		bMovementCancelOpen = true;
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

	FVector Direction = GetRootMotionWarpDirection().GetSafeNormal2D();
	float TravelDistance = RootMotionDistance;

	// A leap: aim at the target and stop just short of it, never farther than Root Motion Distance.
	const AActor* WarpTarget = bWarpToAbilityTarget ? GetAbilityTarget() : nullptr;

	if (IsValid(WarpTarget))
	{
		const FVector ToTarget = WarpTarget->GetActorLocation() - Character->GetActorLocation();
		const float EdgeGap = ToTarget.Size2D() - Character->GetSimpleCollisionRadius() - WarpTarget->GetSimpleCollisionRadius();

		Direction = ToTarget.GetSafeNormal2D();
		TravelDistance = FMath::Clamp(EdgeGap - TargetStopDistance, 0.0f, RootMotionDistance);
	}

	// Zero direction (or zero distance) collapses the target onto the character,
	// which warps the montage to translate in place.
	const FVector StartLocation = Character->GetActorLocation();
	const FVector TargetLocation = Direction.IsNearlyZero()
		? StartLocation
		: StartLocation + Direction * TravelDistance;

	// Keep the character's current facing for the warp target rotation; distance
	// warping only needs the translation goal.
	Warping->AddOrUpdateWarpTargetFromLocationAndRotation(
		RootMotionWarpName,
		TargetLocation,
		Character->GetActorRotation()
	);
}
void UMontageAbility::OnMovementInputReceived_Implementation(const FVector2D MovementInput)
{
	Super::OnMovementInputReceived_Implementation(MovementInput);

	if (!bMovementCancelOpen ||
		bEndedByMovement ||
		GetAbilityStatus() != EAbilityStatus::Active ||
		MovementInput.SizeSquared() <= FMath::Square(MovementCancelDeadZone))
	{
		return;
	}

	// The meaningful part is done, so this counts as a normal finish.
	bEndedByMovement = true;
	RequestEndAbility();
}
