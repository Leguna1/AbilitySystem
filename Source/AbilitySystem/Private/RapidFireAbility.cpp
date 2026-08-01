#include "RapidFireAbility.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

URapidFireAbility::URapidFireAbility()
{
	ActivationPriority = 200;
	bCanActivateFromHeldInput = false;
	bRequireInputHeldAtResolution = false;
	bEndAbilityWhenMontageEnds = false;
}

bool URapidFireAbility::CanActivateAbility_Implementation() const
{
	if (!Super::CanActivateAbility_Implementation())
	{
		return false;
	}

	return IsValid(AbilityMontage) &&
		MaximumShots > 0 &&
		ValidateMontageSections();
}

void URapidFireAbility::ActivateAbility_Implementation()
{
	RapidFireStage = ERapidFireStage::Windup;
	ShotsFired = 0;
	bFinishRequested = false;
	bFiringLoopStarted = false;

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
	SectionChangedDelegate.BindUObject(this, &URapidFireAbility::HandleRapidFireSectionChanged);

	AnimInstance->Montage_SetSectionChangedDelegate(SectionChangedDelegate, AbilityMontage);

	if (!ConfigureMontageSections())
	{
		RequestCancelAbility();
		return;
	}

	OnRapidFireStarted();
}

void URapidFireAbility::OnMovementInputReceived_Implementation(const FVector2D MovementInput)
{
	Super::OnMovementInputReceived_Implementation(MovementInput);

	if (MovementInput.SizeSquared() <= FMath::Square(MovementInterruptionDeadZone))
	{
		return;
	}

	switch (MovementResponse)
	{
	case ERapidFireMovementResponse::Ignore:
		return;

	case ERapidFireMovementResponse::Cancel:
		RequestCancelAbility();
		return;

	case ERapidFireMovementResponse::Finish:
		if (RapidFireStage == ERapidFireStage::Firing)
		{
			RequestFinishRapidFire();
		}
		else if (RapidFireStage == ERapidFireStage::Windup)
		{
			RequestCancelAbility();
		}
		return;

	default:
		return;
	}
}

void URapidFireAbility::OnProjectileReleased_Implementation(const float Strength)
{
	Super::OnProjectileReleased_Implementation(Strength);

	if (RapidFireStage != ERapidFireStage::Firing)
	{
		return;
	}

	++ShotsFired;

	OnRapidFireShotReleased(
		ShotsFired,
		MaximumShots,
		Strength
	);

	if (ShotsFired >= MaximumShots)
	{
		RequestFinishRapidFire();
		return;
	}

	if (bFinishRequested)
	{
		return;
	}

	/*
	 * The released projectile has left the bow. Reset only the per-shot state
	 * so the next FireLoop iteration may prepare another projectile.
	 */
	ResetProjectileCycle();
}

void URapidFireAbility::OnAbilityMontageEnded_Implementation(UAnimMontage* Montage, const bool bInterrupted)
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

	if (RapidFireStage != ERapidFireStage::Recovery)
	{
		RequestCancelAbility();
		return;
	}

	RequestEndAbility();
}

void URapidFireAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	Super::OnAbilityEnded_Implementation(EndReason);

	RapidFireStage = ERapidFireStage::Inactive;
	ShotsFired = 0;
	bFinishRequested = false;
	bFiringLoopStarted = false;
}

bool URapidFireAbility::CanReplaceActiveAbility_Implementation(const UAbility* CurrentAbility) const
{
	return true;
}

FAbilityProgress URapidFireAbility::GetAbilityProgress_Implementation() const
{
	FAbilityProgress Progress;

	// Only show once actually firing; hide during windup/recovery/inactive.
	if (RapidFireStage != ERapidFireStage::Firing)
	{
		return Progress; // Kind == None
	}

	Progress.Kind = EAbilityProgressKind::Count;
	Progress.Current = GetRemainingShots();
	Progress.Max = MaximumShots;
	// Deplete 1 -> 0 as shots are spent.
	Progress.Normalized = 1.0f - GetRapidFireProgress();
	return Progress;
}

float URapidFireAbility::GetRapidFireProgress() const
{
	if (MaximumShots <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		static_cast<float>(ShotsFired) / static_cast<float>(MaximumShots),
		0.0f,
		1.0f
	);
}

bool URapidFireAbility::RequestFinishRapidFire()
{
	if (RapidFireStage != ERapidFireStage::Firing ||
		bFinishRequested ||
		IsCommitted())
	{
		return false;
	}

	UAnimInstance* AnimInstance = GetAnimInstance();

	if (!IsValid(AnimInstance) || !IsValid(AbilityMontage))
	{
		RequestCancelAbility();
		return false;
	}

	/*
	 * Rapid Fire becomes committed when it decides to stop producing shots
	 * and proceed through its recovery.
	 */
	if (!RequestCommit())
	{
		RequestCancelAbility();
		return false;
	}

	bFinishRequested = true;

	/*
	 * Finish the current FireLoop iteration, then enter Recovery instead of
	 * repeating the loop.
	 */
	AnimInstance->Montage_SetNextSection(
		FireLoopSectionName,
		RecoverySectionName,
		AbilityMontage
	);

	OnRapidFireFinishRequested();
	return true;
}

bool URapidFireAbility::ConfigureMontageSections()
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
		FireLoopSectionName,
		AbilityMontage
	);

	AnimInstance->Montage_SetNextSection(
		FireLoopSectionName,
		FireLoopSectionName,
		AbilityMontage
	);

	return AnimInstance->Montage_GetCurrentSection(AbilityMontage) == DefaultSectionName;
}

bool URapidFireAbility::ValidateMontageSections() const
{
	if (!IsValid(AbilityMontage) ||
		DefaultSectionName.IsNone() ||
		FireLoopSectionName.IsNone() ||
		RecoverySectionName.IsNone())
	{
		return false;
	}

	if (DefaultSectionName == FireLoopSectionName ||
		DefaultSectionName == RecoverySectionName ||
		FireLoopSectionName == RecoverySectionName)
	{
		return false;
	}

	return AbilityMontage->GetSectionIndex(DefaultSectionName) == 0 &&
		AbilityMontage->GetSectionIndex(FireLoopSectionName) != INDEX_NONE &&
		AbilityMontage->GetSectionIndex(RecoverySectionName) != INDEX_NONE;
}

void URapidFireAbility::HandleRapidFireSectionChanged(UAnimMontage* Montage, const FName SectionName, const bool bLooped)
{
	if (Montage != AbilityMontage ||
		GetAbilityStatus() != EAbilityStatus::Active)
	{
		return;
	}

	if (SectionName == FireLoopSectionName)
	{
		RapidFireStage = ERapidFireStage::Firing;

		if (!bFiringLoopStarted)
		{
			bFiringLoopStarted = true;
			OnFiringLoopStarted();
		}

		return;
	}

	if (SectionName == RecoverySectionName)
	{
		RapidFireStage = ERapidFireStage::Recovery;

		/*
		 * A malformed montage might enter Recovery before its final projectile
		 * is released. Return any remaining projectile safely.
		 */
		DiscardPreparedProjectile();

		OnRapidFireRecoveryStarted();
	}
}

void URapidFireAbility::OnRapidFireStarted_Implementation()
{
}

void URapidFireAbility::OnFiringLoopStarted_Implementation()
{
}

void URapidFireAbility::OnRapidFireShotReleased_Implementation(int32 ShotNumber, int32 MaxShots, float Strength)
{
}

void URapidFireAbility::OnRapidFireFinishRequested_Implementation()
{
}

void URapidFireAbility::OnRapidFireRecoveryStarted_Implementation()
{
}