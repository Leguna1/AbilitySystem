#include "ReviveCompanionAbility.h"

#include "CompanionCharacter.h"
#include "CompanionComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

UReviveCompanionAbility::UReviveCompanionAbility()
{
	// Commits as soon as it starts; acts then (or at Act Event Tag).
	CostTrigger = EAbilityCostTrigger::OnActivate;
}

UCompanionComponent* UReviveCompanionAbility::GetCompanionComponent() const
{
	const ACharacter* OwnerCharacter = GetOwningCharacter();
	return IsValid(OwnerCharacter) ? OwnerCharacter->FindComponentByClass<UCompanionComponent>() : nullptr;
}

bool UReviveCompanionAbility::CanActivateAbility_Implementation() const
{
	const UCompanionComponent* Companions = GetCompanionComponent();
	return Super::CanActivateAbility_Implementation() && IsValid(Companions) && Companions->CanReviveCompanion(MaxRange);
}

void UReviveCompanionAbility::ActivateAbility_Implementation()
{
	bActed = false;
	bInstant = !IsValid(SelectAbilityMontage());

	if (!bInstant)
	{
		Super::ActivateAbility_Implementation();
		return;
	}

	// No montage: act on commit (right after this), then end on the next tick.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UReviveCompanionAbility::EndInstantUse));
	}
}

void UReviveCompanionAbility::EndInstantUse()
{
	if (GetAbilityStatus() == EAbilityStatus::Active)
	{
		if (IsCommitted())
		{
			Act();
		}

		RequestEndAbility();
	}
}

void UReviveCompanionAbility::OnAbilityCommitted_Implementation()
{
	Super::OnAbilityCommitted_Implementation();

	if (!ActEventTag.IsValid() || bInstant)
	{
		Act();
	}
}

void UReviveCompanionAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (ActEventTag.IsValid() && EventTag.MatchesTagExact(ActEventTag))
	{
		Act();
	}
}

void UReviveCompanionAbility::Act()
{
	UCompanionComponent* Companions = GetCompanionComponent();

	if (bActed || !IsValid(Companions))
	{
		return;
	}

	bActed = true;

	// The body may have decayed during the montage: then nothing happens (the cost is already paid).
	if (ACompanionCharacter* Revived = Companions->ReviveCompanion(HealthFractionByRank.Get(GetAbilityRank())))
	{
		OnCompanionRevived(Revived);
	}
}
