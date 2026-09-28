#include "ActiveAbility.h"

#include "AbilityComponent.h"

void UActiveAbility::InitializeAbility(UAbilityComponent* InAbilityComponent, ACharacter* InOwningCharacter)
{
	Super::InitializeAbility(InAbilityComponent, InOwningCharacter);

	const UAbilityComponent* Component = GetAbilityComponent();

	TargetingComponent = IsValid(Component) ? Component->GetTargetingComponent() : nullptr;
	MotionWarpingComponent = IsValid(Component) ? Component->GetMotionWarpingComponent() : nullptr;

	AbilityStatus = EAbilityStatus::Inactive;
	bCommitted = false;
	bTransitionOpen = false;
	bEarlyCancellationClosed = false;
}

bool UActiveAbility::CanActivateAbility_Implementation() const
{
	return true;
}

void UActiveAbility::ActivateAbility_Implementation()
{
}

bool UActiveAbility::CanCommitAbility_Implementation() const
{
	return true;
}

void UActiveAbility::OnAbilityCommitted_Implementation()
{
}

void UActiveAbility::OnCostPaymentFailed_Implementation()
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->EndAbility(this, EAbilityEndReason::Failed);
	}
}

void UActiveAbility::OnInputPressed_Implementation(FGameplayTag InputTag)
{
}

void UActiveAbility::OnInputReleased_Implementation(FGameplayTag InputTag)
{
}

void UActiveAbility::OnMovementInputReceived_Implementation(FVector2D MovementInput)
{
}

void UActiveAbility::OnAnimationEvent_Implementation(FGameplayTag EventTag)
{
}

bool UActiveAbility::CanHandleRepeatedActivationRequest_Implementation() const
{
	return false;
}

bool UActiveAbility::HandleRepeatedActivationRequest_Implementation()
{
	return false;
}

bool UActiveAbility::CanTransitionTo_Implementation(const UActiveAbility* IncomingAbility) const
{
	if (!bTransitionOpen || !IsValid(IncomingAbility) || AllowedTransitionAbilityTags.IsEmpty())
	{
		return false;
	}

	return AllowedTransitionAbilityTags.HasAny(IncomingAbility->GetAbilityTags());
}

bool UActiveAbility::CanEarlyCancelTo_Implementation(const UActiveAbility* IncomingAbility) const
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

bool UActiveAbility::CanBeCancelledBy_Implementation(const UActiveAbility* IncomingAbility) const
{
	return false;
}

bool UActiveAbility::CanReplaceActiveAbility_Implementation(const UActiveAbility* CurrentAbility) const
{
	return false;
}

void UActiveAbility::TickAbility_Implementation(float DeltaTime)
{
}

void UActiveAbility::OnAbilityEnded_Implementation(EAbilityEndReason EndReason)
{
}

bool UActiveAbility::RequestCommit()
{
	UAbilityComponent* Component = GetAbilityComponent();
	return IsValid(Component) && Component->CommitAbility(this);
}

void UActiveAbility::RequestEndAbility()
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->EndAbility(this, EAbilityEndReason::Completed);
	}
}

void UActiveAbility::RequestCancelAbility()
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->EndAbility(this, EAbilityEndReason::Cancelled);
	}
}

bool UActiveAbility::RequestAbility(const FGameplayTag RequestedAbilityId)
{
	UAbilityComponent* Component = GetAbilityComponent();
	return IsValid(Component) && Component->TryActivateAbility(RequestedAbilityId);
}

bool UActiveAbility::RequestResolveBufferedInput()
{
	UAbilityComponent* Component = GetAbilityComponent();
	return IsValid(Component) && Component->ResolveBufferedAbilityInput();
}

void UActiveAbility::ClearBufferedInputs()
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->ClearBufferedInputs();
	}
}

void UActiveAbility::OpenTransition()
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->SetAbilityTransitionOpen(this, true);
	}
}

void UActiveAbility::CloseTransition()
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->SetAbilityTransitionOpen(this, false);
	}
}

void UActiveAbility::CloseEarlyCancellation()
{
	if (const UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->SetAbilityEarlyCancellationClosed(this, true);
	}
}

void UActiveAbility::SetAbilityTickEnabled(const bool bEnabled)
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->SetAbilityTickEnabled(this, bEnabled);
	}
}

bool UActiveAbility::IsInputHeld(const FGameplayTag InputTag) const
{
	const UAbilityComponent* Component = GetAbilityComponent();
	return IsValid(Component) && Component->IsInputHeld(InputTag);
}

FVector2D UActiveAbility::GetMovementInput() const
{
	const UAbilityComponent* Component = GetAbilityComponent();
	return IsValid(Component) ? Component->GetMovementInput() : FVector2D::ZeroVector;
}