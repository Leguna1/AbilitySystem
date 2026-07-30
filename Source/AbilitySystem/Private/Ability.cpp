#include "Ability.h"

#include "AbilityComponent.h"
#include "GameFramework/Character.h"

void UAbility::InitializeAbility(UAbilityComponent* InAbilityComponent, ACharacter* InOwningCharacter)
{
	AbilityComponent = InAbilityComponent;
	OwningCharacter = InOwningCharacter;

	TargetingComponent = IsValid(AbilityComponent)
		? AbilityComponent->GetTargetingComponent()
		: nullptr;

	MotionWarpingComponent = IsValid(AbilityComponent)
		? AbilityComponent->GetMotionWarpingComponent()
		: nullptr;

	AbilityStatus = EAbilityStatus::Inactive;
	bCommitted = false;
	bTransitionOpen = false;
	bEarlyCancellationClosed = false;
}

UWorld* UAbility::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	return IsValid(AbilityComponent) ? AbilityComponent->GetWorld() : nullptr;
}

bool UAbility::CanActivateAbility_Implementation() const
{
	return true;
}

void UAbility::ActivateAbility_Implementation()
{
}

bool UAbility::CanCommitAbility_Implementation() const
{
	return true;
}

void UAbility::OnAbilityCommitted_Implementation()
{
}

void UAbility::OnInputPressed_Implementation(FGameplayTag InputTag)
{
}

void UAbility::OnInputReleased_Implementation(FGameplayTag InputTag)
{
}

void UAbility::OnMovementInputReceived_Implementation(FVector2D MovementInput)
{
}

void UAbility::OnAnimationEvent_Implementation(FGameplayTag EventTag)
{
}

bool UAbility::CanHandleRepeatedActivationRequest_Implementation() const
{
	return false;
}

bool UAbility::HandleRepeatedActivationRequest_Implementation()
{
	return false;
}

bool UAbility::CanTransitionTo_Implementation(const UAbility* IncomingAbility) const
{
	if (!bTransitionOpen || !IsValid(IncomingAbility))
	{
		return false;
	}

	if (AllowedTransitionAbilityTags.IsEmpty())
	{
		return false;
	}

	return AllowedTransitionAbilityTags.HasAny(IncomingAbility->GetAbilityTags());
}

bool UAbility::CanEarlyCancelTo_Implementation(const UAbility* IncomingAbility) const
{
	if (bEarlyCancellationClosed ||
		bCommitted ||
		!IsValid(IncomingAbility) ||
		AllowedEarlyCancellationAbilityTags.IsEmpty())
	{
		return false;
	}

	return AllowedEarlyCancellationAbilityTags.HasAny(IncomingAbility->GetAbilityTags());
}

bool UAbility::CanBeCancelledBy_Implementation(const UAbility* IncomingAbility) const
{
	return false;
}

bool UAbility::CanReplaceActiveAbility_Implementation(const UAbility* CurrentAbility) const
{
	return false;
}

void UAbility::TickAbility_Implementation(float DeltaTime)
{
}

void UAbility::OnAbilityEnded_Implementation(EAbilityEndReason EndReason)
{
}

bool UAbility::RequestCommit()
{
	return IsValid(AbilityComponent) && AbilityComponent->CommitAbility(this);
}

void UAbility::RequestEndAbility()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->EndAbility(this, EAbilityEndReason::Completed);
	}
}

void UAbility::RequestCancelAbility()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->EndAbility(this, EAbilityEndReason::Cancelled);
	}
}

bool UAbility::RequestAbility(FGameplayTag RequestedAbilityId)
{
	return IsValid(AbilityComponent) && AbilityComponent->TryActivateAbility(RequestedAbilityId);
}

bool UAbility::RequestResolveBufferedInput()
{
	return IsValid(AbilityComponent) && AbilityComponent->ResolveBufferedAbilityInput();
}

void UAbility::ClearBufferedInputs()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->ClearBufferedInputs();
	}
}

void UAbility::OpenTransition()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->SetAbilityTransitionOpen(this, true);
	}
}

void UAbility::CloseTransition()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->SetAbilityTransitionOpen(this, false);
	}
}

void UAbility::CloseEarlyCancellation()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->SetAbilityEarlyCancellationClosed(this, true);
	}
}

void UAbility::SetAbilityTickEnabled(bool bEnabled)
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->SetAbilityTickEnabled(this, bEnabled);
	}
}

bool UAbility::IsInputHeld(FGameplayTag InputTag) const
{
	return IsValid(AbilityComponent) && AbilityComponent->IsInputHeld(InputTag);
}

FVector2D UAbility::GetMovementInput() const
{
	return IsValid(AbilityComponent)
		? AbilityComponent->GetMovementInput()
		: FVector2D::ZeroVector;
}

bool UAbility::OwnerHasTag(FGameplayTag Tag) const
{
	return IsValid(AbilityComponent) && AbilityComponent->HasOwnerTag(Tag);
}

bool UAbility::OwnerHasAllTags(const FGameplayTagContainer& Tags) const
{
	return IsValid(AbilityComponent) && AbilityComponent->HasAllOwnerTags(Tags);
}

bool UAbility::OwnerHasAnyTags(const FGameplayTagContainer& Tags) const
{
	return IsValid(AbilityComponent) && AbilityComponent->HasAnyOwnerTags(Tags);
}
