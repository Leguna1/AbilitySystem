#include "SummonCompanionAbility.h"

#include "CompanionCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "SummonCompanionAbility"

USummonCompanionAbility::USummonCompanionAbility()
{
	// The cooldown is the respawn wait: started by the Companion Component when the companion dies.
	CooldownTrigger = EAbilityCooldownTrigger::Manual;
	CooldownDuration = 30.0f;

	// Commits as soon as it starts, which is when it acts.
	CostTrigger = EAbilityCostTrigger::OnActivate;
}

UCompanionComponent* USummonCompanionAbility::GetCompanionComponent() const
{
	const ACharacter* OwnerCharacter = GetOwningCharacter();
	return IsValid(OwnerCharacter) ? OwnerCharacter->FindComponentByClass<UCompanionComponent>() : nullptr;
}

bool USummonCompanionAbility::IsCompanionOut() const
{
	const UCompanionComponent* Companions = GetCompanionComponent();
	return IsValid(Companions) && Companions->HasCompanion();
}

bool USummonCompanionAbility::CanActivateAbility_Implementation() const
{
	return Super::CanActivateAbility_Implementation() && IsValid(GetCompanionComponent()) && CompanionClass != nullptr;
}

void USummonCompanionAbility::ActivateAbility_Implementation()
{
	bSummonThisUse = !IsCompanionOut();
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
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &USummonCompanionAbility::EndInstantUse));
	}
}

void USummonCompanionAbility::EndInstantUse()
{
	if (GetAbilityStatus() == EAbilityStatus::Active)
	{
		// Only a paid use acts (the commit can fail on cost).
		if (IsCommitted())
		{
			Act();
		}

		RequestEndAbility();
	}
}

void USummonCompanionAbility::OnAbilityCommitted_Implementation()
{
	Super::OnAbilityCommitted_Implementation();

	if (!ActEventTag.IsValid() || bInstant)
	{
		Act();
	}
}

void USummonCompanionAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (ActEventTag.IsValid() && EventTag.MatchesTagExact(ActEventTag))
	{
		Act();
	}
}

void USummonCompanionAbility::Act()
{
	UCompanionComponent* Companions = GetCompanionComponent();

	if (bActed || !IsValid(Companions))
	{
		return;
	}

	bActed = true;

	if (bSummonThisUse)
	{
		const int32 Rank = GetAbilityRank();

		ACompanionCharacter* Summoned = Companions->SpawnCompanion(CompanionClass, FindSpawnLocation(), LevelOffsetByRank.Get(Rank),
			AbilityGrants, GetAbilityId());

		OnCompanionAction(true, Summoned);
		return;
	}

	if (WhenAlreadyOut == ECompanionOutAction::Dismiss)
	{
		Companions->DismissCompanion();
		OnCompanionAction(false, nullptr);
		return;
	}

	Companions->Recall();
	OnCompanionAction(false, Companions->GetCompanion());
}

FVector USummonCompanionAbility::FindSpawnLocation() const
{
	const ACharacter* OwnerCharacter = GetOwningCharacter();

	// To your side first, then around you: wherever there's ground at your height.
	return IsValid(OwnerCharacter)
		? UCompanionComponent::FindPlacementNearLeader(OwnerCharacter, OwnerCharacter->GetActorRightVector(), SpawnDistance)
		: FVector::ZeroVector;
}

#if WITH_EDITOR
EDataValidationResult USummonCompanionAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!CompanionClass)
	{
		Context.AddError(LOCTEXT("NoClass", "Companion Class is empty: there's nothing to summon."));
		Result = EDataValidationResult::Invalid;
	}

	if (GetCooldownTrigger() != EAbilityCooldownTrigger::Manual)
	{
		Context.AddWarning(LOCTEXT("NotManual", "Cooldown Trigger isn't Manual: summoning and recalling will start the respawn cooldown too."));
	}

	for (int32 Index = 0; Index < AbilityGrants.Num(); ++Index)
	{
		if (!AbilityGrants[Index].CompanionAbility)
		{
			Context.AddError(FText::Format(LOCTEXT("NoGrant", "Ability Grants [{0}]: Companion Ability is empty."), Index));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif

#undef LOCTEXT_NAMESPACE
