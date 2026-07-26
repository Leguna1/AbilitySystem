#include "RangedComboAttackAbility.h"

#include "Animation/AnimMontage.h"

URangedComboAttackAbility::URangedComboAttackAbility()
{
	ActivationPriority = 100;
	bCanActivateFromHeldInput = true;
	bRequireInputHeldAtResolution = true;
	bEndAbilityWhenMontageEnds = false;
}

bool URangedComboAttackAbility::CanActivateAbility_Implementation() const
{
	if (!Super::CanActivateAbility_Implementation())
	{
		return false;
	}

	return ComboMontages.IsValidIndex(0) &&
		IsValid(ComboMontages[0]);
}

void URangedComboAttackAbility::ActivateAbility_Implementation()
{
	CurrentComboIndex = 0;
	bChangingComboStep = false;

	Super::ActivateAbility_Implementation();

	if (GetAbilityStatus() == EAbilityStatus::Active &&
		IsAbilityMontagePlaying())
	{
		OnComboStepStarted(CurrentComboIndex);
	}
}

void URangedComboAttackAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (InputCheckPointEventTag.IsValid() &&
		EventTag.MatchesTagExact(InputCheckPointEventTag))
	{
		RequestResolveBufferedInput();
	}
}

bool URangedComboAttackAbility::CanHandleRepeatedActivationRequest_Implementation() const
{
	if (ComboMontages.Num() <= 1)
	{
		return false;
	}

	return bLoopCombo || !IsFinalComboStep();
}

bool URangedComboAttackAbility::HandleRepeatedActivationRequest_Implementation()
{
	if (!CanHandleRepeatedActivationRequest())
	{
		return false;
	}

	const int32 PreviousComboIndex = CurrentComboIndex;

	if (IsFinalComboStep())
	{
		CurrentComboIndex = 0;
	}
	else
	{
		++CurrentComboIndex;
	}

	ResetProjectileCycle();

	bChangingComboStep = true;
	const bool bPlayed = PlayCurrentComboStep();
	bChangingComboStep = false;

	if (!bPlayed)
	{
		CurrentComboIndex = PreviousComboIndex;
		return false;
	}

	OnComboAdvanced(PreviousComboIndex, CurrentComboIndex);
	OnComboStepStarted(CurrentComboIndex);

	return true;
}

void URangedComboAttackAbility::OnAbilityMontageEnded_Implementation(UAnimMontage* Montage, const bool bInterrupted)
{
	if (bChangingComboStep)
	{
		return;
	}

	if (!ComboMontages.IsValidIndex(CurrentComboIndex) ||
		Montage != ComboMontages[CurrentComboIndex])
	{
		return;
	}

	if (bInterrupted)
	{
		RequestCancelAbility();
		return;
	}

	OnComboStepFinished(CurrentComboIndex);
	RequestEndAbility();
}

void URangedComboAttackAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	Super::OnAbilityEnded_Implementation(EndReason);

	CurrentComboIndex = 0;
	bChangingComboStep = false;
}

UAnimMontage* URangedComboAttackAbility::SelectAbilityMontage_Implementation() const
{
	return ComboMontages.IsValidIndex(CurrentComboIndex)
		? ComboMontages[CurrentComboIndex]
		: nullptr;
}

bool URangedComboAttackAbility::PlayCurrentComboStep()
{
	UAnimMontage* Montage = SelectAbilityMontage();

	return IsValid(Montage) &&
		PlayAbilityMontage(Montage, MontagePlayRate);
}

bool URangedComboAttackAbility::IsFinalComboStep() const
{
	return ComboMontages.IsEmpty() ||
		CurrentComboIndex >= ComboMontages.Num() - 1;
}

void URangedComboAttackAbility::OnComboStepStarted_Implementation(int32 ComboIndex)
{
}

void URangedComboAttackAbility::OnComboStepFinished_Implementation(int32 ComboIndex)
{
}

void URangedComboAttackAbility::OnComboAdvanced_Implementation(int32 PreviousComboIndex, int32 NewComboIndex)
{
}