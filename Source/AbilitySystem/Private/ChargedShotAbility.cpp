#include "ChargedShotAbility.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

UChargedShotAbility::UChargedShotAbility()
{
	ActivationPriority = 200;
	bCanActivateFromHeldInput = false;
	bRequireInputHeldAtResolution = false;
	bEndAbilityWhenMontageEnds = false;
}

bool UChargedShotAbility::CanActivateAbility_Implementation() const
{
	if (!Super::CanActivateAbility_Implementation())
	{
		return false;
	}

	return IsValid(AbilityMontage) &&
		MaximumChargeTime > 0.0f &&
		ValidateMontageSections();
}

void UChargedShotAbility::ActivateAbility_Implementation()
{
	ChargedShotStage = EChargedShotStage::Windup;
	CurrentChargeTime = 0.0f;
	bChargeStarted = false;
	bMaximumChargeReached = false;

	Super::ActivateAbility_Implementation();

	if (GetAbilityStatus() != EAbilityStatus::Active ||
		!IsAbilityMontagePlaying())
	{
		return;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();

	if (!IsValid(AnimInstance))
	{
		RequestCancelAbility();
		return;
	}

	FOnMontageSectionChanged SectionChangedDelegate;
	SectionChangedDelegate.BindUObject(this, &UChargedShotAbility::HandleChargedShotSectionChanged);

	AnimInstance->Montage_SetSectionChangedDelegate(SectionChangedDelegate, AbilityMontage);

	if (!ConfigureMontageSections())
	{
		RequestCancelAbility();
		return;
	}

	OnWindupStarted();
}

void UChargedShotAbility::TickAbility_Implementation(const float DeltaTime)
{
	if (ChargedShotStage != EChargedShotStage::Charging ||
		!bChargeStarted ||
		bMaximumChargeReached ||
		IsCommitted())
	{
		SetAbilityTickEnabled(false);
		return;
	}

	CurrentChargeTime = FMath::Min(
		CurrentChargeTime + FMath::Max(DeltaTime, 0.0f),
		MaximumChargeTime
	);

	OnChargeUpdated(
		CurrentChargeTime,
		MaximumChargeTime,
		GetNormalizedCharge()
	);

	if (CurrentChargeTime >= MaximumChargeTime)
	{
		HandleMaximumCharge();
	}
}

void UChargedShotAbility::OnMovementInputReceived_Implementation(const FVector2D MovementInput)
{
	Super::OnMovementInputReceived_Implementation(MovementInput);

	if (IsCommitted() ||
		MovementInput.SizeSquared() <= FMath::Square(MovementInterruptionDeadZone))
	{
		return;
	}

	const bool bInterruptibleStage =
		ChargedShotStage == EChargedShotStage::Windup ||
		ChargedShotStage == EChargedShotStage::Charging;

	if (!bInterruptibleStage)
	{
		return;
	}

	switch (MovementInterruptionResponse)
	{
	case EChargeInterruptionResponse::Ignore:
		return;

	case EChargeInterruptionResponse::Cancel:
		SetAbilityTickEnabled(false);
		RequestCancelAbility();
		return;

	case EChargeInterruptionResponse::Release:
		/*
		 * Recovery assumes that charging has begun and that a projectile has
		 * been prepared. Movement during early windup therefore cancels.
		 */
		if (ChargedShotStage != EChargedShotStage::Charging ||
			!HasPreparedProjectile())
		{
			SetAbilityTickEnabled(false);
			RequestCancelAbility();
			return;
		}

		RequestChargedRelease(true);
		return;

	default:
		return;
	}
}

void UChargedShotAbility::OnAbilityMontageEnded_Implementation(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != AbilityMontage)
	{
		return;
	}

	if (bInterrupted)
	{
		RequestCancelAbility();
		return;
	}

	if (ChargedShotStage != EChargedShotStage::Recovery)
	{
		RequestCancelAbility();
		return;
	}

	RequestEndAbility();
}

void UChargedShotAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	SetAbilityTickEnabled(false);

	Super::OnAbilityEnded_Implementation(EndReason);

	ChargedShotStage = EChargedShotStage::Inactive;
	CurrentChargeTime = 0.0f;
	bChargeStarted = false;
	bMaximumChargeReached = false;
}

bool UChargedShotAbility::CanReplaceActiveAbility_Implementation(const UAbility* CurrentAbility) const
{
	return true;
}

float UChargedShotAbility::GetNormalizedCharge() const
{
	if (MaximumChargeTime <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		CurrentChargeTime / MaximumChargeTime,
		0.0f,
		1.0f
	);
}

float UChargedShotAbility::ResolveProjectileStrength_Implementation() const
{
	return GetNormalizedCharge();
}

bool UChargedShotAbility::RequestChargedRelease(const bool bImmediate)
{
	if (ChargedShotStage != EChargedShotStage::Charging ||
		!bChargeStarted ||
		IsCommitted() ||
		!HasPreparedProjectile())
	{
		return false;
	}

	if (!RequestCommit())
	{
		RequestCancelAbility();
		return false;
	}

	SetAbilityTickEnabled(false);

	UAnimInstance* AnimInstance = GetAnimInstance();

	if (!IsValid(AnimInstance) || !IsValid(AbilityMontage))
	{
		RequestCancelAbility();
		return false;
	}

	if (bImmediate)
	{
		AnimInstance->Montage_JumpToSection(
			RecoverySectionName,
			AbilityMontage
		);
	}
	else
	{
		AnimInstance->Montage_SetNextSection(
			ChargeSectionName,
			RecoverySectionName,
			AbilityMontage
		);
	}

	OnChargeReleaseRequested(bImmediate);
	return true;
}

bool UChargedShotAbility::HandleMaximumCharge()
{
	if (ChargedShotStage != EChargedShotStage::Charging ||
		bMaximumChargeReached)
	{
		return false;
	}

	CurrentChargeTime = MaximumChargeTime;
	bMaximumChargeReached = true;

	OnChargeUpdated(
		CurrentChargeTime,
		MaximumChargeTime,
		1.0f
	);

	OnMaximumChargeReached();

	if (bAutoReleaseAtMaximumCharge)
	{
		return RequestChargedRelease(false);
	}

	/*
	 * Remain in the looping Charge section at maximum strength until another
	 * release source calls RequestChargedRelease().
	 */
	SetAbilityTickEnabled(false);
	return true;
}

bool UChargedShotAbility::ConfigureMontageSections()
{
	UAnimInstance* AnimInstance = GetAnimInstance();

	if (!IsValid(AnimInstance) ||
		!IsValid(AbilityMontage) ||
		!ValidateMontageSections())
	{
		return false;
	}

	AnimInstance->Montage_SetNextSection(
		DefaultSectionName,
		ChargeSectionName,
		AbilityMontage
	);

	AnimInstance->Montage_SetNextSection(
		ChargeSectionName,
		ChargeSectionName,
		AbilityMontage
	);

	return AnimInstance->Montage_GetCurrentSection(AbilityMontage) == DefaultSectionName;
}

bool UChargedShotAbility::ValidateMontageSections() const
{
	if (!IsValid(AbilityMontage) ||
		DefaultSectionName.IsNone() ||
		ChargeSectionName.IsNone() ||
		RecoverySectionName.IsNone())
	{
		return false;
	}

	if (DefaultSectionName == ChargeSectionName ||
		DefaultSectionName == RecoverySectionName ||
		ChargeSectionName == RecoverySectionName)
	{
		return false;
	}

	return AbilityMontage->GetSectionIndex(DefaultSectionName) == 0 &&
		AbilityMontage->GetSectionIndex(ChargeSectionName) != INDEX_NONE &&
		AbilityMontage->GetSectionIndex(RecoverySectionName) != INDEX_NONE;
}

void UChargedShotAbility::HandleChargedShotSectionChanged(UAnimMontage* Montage, const FName SectionName, const bool bLooped)
{
	if (Montage != AbilityMontage ||
		GetAbilityStatus() != EAbilityStatus::Active)
	{
		return;
	}

	if (SectionName == ChargeSectionName && !bChargeStarted)
	{
		bChargeStarted = true;
		ChargedShotStage = EChargedShotStage::Charging;
		CurrentChargeTime = 0.0f;
		bMaximumChargeReached = false;

		SetAbilityTickEnabled(true);

		OnChargeStarted();
		OnChargeUpdated(
			CurrentChargeTime,
			MaximumChargeTime,
			0.0f
		);

		return;
	}

	if (SectionName == RecoverySectionName)
	{
		SetAbilityTickEnabled(false);
		ChargedShotStage = EChargedShotStage::Recovery;

		OnRecoveryStarted();
	}
}

void UChargedShotAbility::OnWindupStarted_Implementation()
{
}

void UChargedShotAbility::OnChargeStarted_Implementation()
{
}

void UChargedShotAbility::OnChargeUpdated_Implementation(float ChargeTime, float MaximumTime, float NormalizedCharge)
{
}

void UChargedShotAbility::OnMaximumChargeReached_Implementation()
{
}

void UChargedShotAbility::OnChargeReleaseRequested_Implementation(bool bImmediate)
{
}

void UChargedShotAbility::OnRecoveryStarted_Implementation()
{
}