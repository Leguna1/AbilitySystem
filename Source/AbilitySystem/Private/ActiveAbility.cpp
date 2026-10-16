#include "ActiveAbility.h"
#include "AbilityComponent.h"
#include "AbilityTargetSource.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "TargetableInterface.h"
#include "TargetingComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "ActiveAbility"

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

TArray<FAbilityDisplayStat> UActiveAbility::GetDisplayStats_Implementation(const int32 Rank) const
{
	TArray<FAbilityDisplayStat> Stats = Super::GetDisplayStats_Implementation(Rank);

	if (FocusCost > 0.0f)
	{
		Stats.Emplace(LOCTEXT("FocusLabel", "Focus"), FormatDisplayNumber(FocusCost));
	}

	if (CooldownDuration > 0.0f)
	{
		Stats.Emplace(LOCTEXT("CooldownLabel", "Cooldown"),
			FText::Format(LOCTEXT("CooldownValue", "{0} sec"), FormatDisplayNumber(CooldownDuration)));
	}

	return Stats;
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
void UActiveAbility::OpenActivationWindow()
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->RequestActivationWindow(this);
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
void UActiveAbility::SendGameplayEvent(const FGameplayTag EventTag)
{
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->BroadcastGameplayEvent(EventTag, this);
	}
}
void UActiveAbility::NotifyFragmentsStarted()
{
	for (const UAbilityFragment* Fragment : Fragments)
	{
		if (IsValid(Fragment))
		{
			Fragment->OnExecutionStarted(*this);
		}
	}
}

void UActiveAbility::NotifyFragmentsEnded(const EAbilityEndReason EndReason)
{
	for (const UAbilityFragment* Fragment : Fragments)
	{
		if (IsValid(Fragment))
		{
			Fragment->OnExecutionEnded(*this, EndReason);
		}
	}
}
AActor* UActiveAbility::GetAbilityTarget() const
{
	// The player picks targets through its Targeting Component.
	if (IsValid(TargetingComponent))
	{
		return TargetingComponent->GetCurrentTarget();
	}

	// AI: the controller decides.
	const ACharacter* OwnerCharacter = GetOwningCharacter();
	const AController* OwnerController = IsValid(OwnerCharacter) ? OwnerCharacter->GetController() : nullptr;

	if (IsValid(OwnerController) && OwnerController->Implements<UAbilityTargetSource>())
	{
		return IAbilityTargetSource::Execute_GetAbilityTarget(OwnerController);
	}

	return nullptr;
}

FVector UActiveAbility::GetAbilityTargetAimLocation() const
{
	if (IsValid(TargetingComponent) && TargetingComponent->HasTarget())
	{
		return TargetingComponent->GetCurrentTargetAimLocation();
	}

	const AActor* AbilityTarget = GetAbilityTarget();

	if (!IsValid(AbilityTarget))
	{
		return FVector::ZeroVector;
	}

	return AbilityTarget->Implements<UTargetableInterface>()
		? ITargetableInterface::Execute_GetTargetAimLocation(AbilityTarget)
		: AbilityTarget->GetActorLocation();
}
FGameplayTag UActiveAbility::GetActivationInputTag() const
{
	const UAbilityComponent* Component = GetAbilityComponent();
	return IsValid(Component) ? Component->GetAbilityInputTag(GetAbilityId()) : ActivationInputTag;
}


#if WITH_EDITOR

EDataValidationResult UActiveAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	const bool bPaysOnEvent =
		CostTrigger == EAbilityCostTrigger::OnAnimationEvent ||
		CostTrigger == EAbilityCostTrigger::OnEveryAnimationEvent;

	if (bPaysOnEvent && !GetCostEventTag().IsValid())
	{
		Context.AddError(LOCTEXT("NoCostEvent",
			"Cost Trigger pays on an animation event, but no Cost Event Tag is set and this ability has no default. Set Cost Event Tag."));
		Result = EDataValidationResult::Invalid;
	}

	TArray<const UClass*> SeenTypes;

	for (int32 Index = 0; Index < Fragments.Num(); ++Index)
	{
		const UAbilityFragment* Fragment = Fragments[Index];

		if (!IsValid(Fragment))
		{
			Context.AddWarning(FText::Format(LOCTEXT("EmptyFeature", "Features [{0}] is empty. Pick a feature or remove the entry."), Index));
			continue;
		}

		if (!Fragment->AllowsMultiple() && SeenTypes.Contains(Fragment->GetClass()))
		{
			Context.AddError(FText::Format(LOCTEXT("DuplicateFeature", "{0} is added more than once. Keep one."),
				Fragment->GetClass()->GetDisplayNameText()));
			Result = EDataValidationResult::Invalid;
		}

		SeenTypes.Add(Fragment->GetClass());
		Result = CombineDataValidationResults(Result, Fragment->IsDataValid(Context));
	}

	return Result;
}

#endif
#undef LOCTEXT_NAMESPACE
