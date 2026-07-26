#include "OffensiveAbilityBase.h"

void UOffensiveAbilityBase::ActivateAbility_Implementation()
{
	Super::ActivateAbility_Implementation();

	if (GetAbilityStatus() == EAbilityStatus::Active)
	{
		OnAttackStarted();
	}
}

void UOffensiveAbilityBase::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	Super::OnAbilityEnded_Implementation(EndReason);

	OnAttackFinished(EndReason);
}

void UOffensiveAbilityBase::OnAttackStarted_Implementation()
{
}

void UOffensiveAbilityBase::OnAttackFinished_Implementation(EAbilityEndReason EndReason)
{
}