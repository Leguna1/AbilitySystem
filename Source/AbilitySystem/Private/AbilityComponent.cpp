#include "AbilityComponent.h"

#include "Ability.h"
#include "GameFramework/Character.h"
#include "InputBufferComponent.h"
#include "MotionWarpingComponent.h"
#include "TargetingComponent.h"

UAbilityComponent::UAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAbilityComponent::BeginPlay()
{
	Super::BeginPlay();

	OwningCharacter = Cast<ACharacter>(GetOwner());
	InputBufferComponent = GetOwner()->FindComponentByClass<UInputBufferComponent>();
	TargetingComponent = GetOwner()->FindComponentByClass<UTargetingComponent>();
	MotionWarpingComponent = GetOwner()->FindComponentByClass<UMotionWarpingComponent>();

	if (!IsValid(OwningCharacter))
	{
		UE_LOG(LogTemp, Error, TEXT("UAbilityComponent requires an ACharacter owner."));
		return;
	}

	if (!IsValid(InputBufferComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("UAbilityComponent requires UInputBufferComponent on %s."), *GetNameSafe(GetOwner()));
		return;
	}

	if (!IsValid(MotionWarpingComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAbilityComponent could not find UMotionWarpingComponent on %s."), *GetNameSafe(GetOwner()));
	}

	if (!IsValid(TargetingComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("UAbilityComponent could not find UTargetingComponent on %s."), *GetNameSafe(GetOwner()));
	}

	for (const TSubclassOf<UAbility> AbilityClass : StartingAbilityClasses)
	{
		GrantAbility(AbilityClass);
	}
}

void UAbilityComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ActiveAbility))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Cancelled);
	}

	Super::EndPlay(EndPlayReason);
}

void UAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(ActiveAbility) || !bAbilityTickEnabled || bEndingAbility)
	{
		return;
	}

	UAbility* TickingAbility = ActiveAbility;

	DispatchAbilityCallback([TickingAbility, DeltaTime]()
	{
		TickingAbility->TickAbility(DeltaTime);
	});
}

bool UAbilityComponent::GrantAbility(TSubclassOf<UAbility> AbilityClass)
{
	const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

	if (!IsValid(AbilityCDO) || !AbilityCDO->GetAbilityId().IsValid())
	{
		return false;
	}

	if (HasAbility(AbilityCDO->GetAbilityId()))
	{
		return true;
	}

	GrantedAbilityClasses.Add(AbilityClass);
	return true;
}

bool UAbilityComponent::RemoveAbility(FGameplayTag AbilityId)
{
	const TSubclassOf<UAbility> AbilityClass = FindAbilityClassById(AbilityId);

	if (!AbilityClass)
	{
		return false;
	}

	if (IsValid(ActiveAbility) && ActiveAbility->GetAbilityId().MatchesTagExact(AbilityId))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Cancelled);
	}

	GrantedAbilityClasses.Remove(AbilityClass);
	return true;
}

bool UAbilityComponent::HasAbility(FGameplayTag AbilityId) const
{
	return FindAbilityClassById(AbilityId) != nullptr;
}

TSubclassOf<UAbility> UAbilityComponent::FindAbilityClassById(const FGameplayTag AbilityId) const
{
	if (!AbilityId.IsValid())
	{
		return nullptr;
	}

	for (const TSubclassOf<UAbility> AbilityClass : GrantedAbilityClasses)
	{
		const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

		if (IsValid(AbilityCDO) && AbilityCDO->GetAbilityId().MatchesTagExact(AbilityId))
		{
			return AbilityClass;
		}
	}

	return nullptr;
}

bool UAbilityComponent::TryActivateAbility(FGameplayTag AbilityId)
{
	const TSubclassOf<UAbility> AbilityClass = FindAbilityClassById(AbilityId);

	if (!AbilityClass || bEndingAbility)
	{
		return false;
	}

	if (IsValid(ActiveAbility))
	{
		if (ActiveAbility->GetClass() == AbilityClass)
		{
			return ExecuteRepeatedAbilityRequest(ActiveAbility);
		}

		return ReplaceActiveAbility(AbilityClass);
	}

	UAbility* NewAbility = CreateExecutionInstance(AbilityClass);

	if (!IsValid(NewAbility) || !CanActivateAbilityInstance(NewAbility, false))
	{
		return false;
	}

	return ActivateAbilityInstance(NewAbility);
}

bool UAbilityComponent::ResolveBufferedAbilityInput()
{
	if (!IsValid(InputBufferComponent) || bResolvingBufferedInput || bEndingAbility)
	{
		return false;
	}

	bResolvingBufferedInput = true;

	TArray<FAbilityInputCandidate> Candidates;
	BuildAbilityInputCandidates(Candidates);

	bool bExecuted = false;

	for (const FAbilityInputCandidate& Candidate : Candidates)
	{
		if (!ExecuteAbilityInputCandidate(Candidate))
		{
			continue;
		}

		InputBufferComponent->ConsumeInput(Candidate.BufferedInput);
		bExecuted = true;
		break;
	}

	bResolvingBufferedInput = false;
	return bExecuted;
}

void UAbilityComponent::CancelActiveAbility()
{
	if (IsValid(ActiveAbility))
	{
		EndActiveAbilityInternal(EAbilityEndReason::Cancelled);
	}
}

void UAbilityComponent::InputPressed(const FGameplayTag InputTag)
{
	if (!IsValid(InputBufferComponent) || !InputTag.IsValid())
	{
		return;
	}

	const float BufferDuration = GetLongestBufferDurationForInput(InputTag);

	if (!InputBufferComponent->InputPressed(InputTag, BufferDuration))
	{
		return;
	}

	if (!IsValid(ActiveAbility) || bEndingAbility)
	{
		ResolveBufferedAbilityInput();
		return;
	}

	UAbility* InputAbility = ActiveAbility;

	DispatchAbilityCallback([InputAbility, InputTag]()
	{
		InputAbility->OnInputPressed(InputTag);
	});

	if (!IsValid(ActiveAbility) ||
		bEndingAbility ||
		bResolvingBufferedInput)
	{
		return;
	}

	/*
	 * Repeated input belonging to the active ability remains controlled by
	 * that ability's InputCheckPoint logic. This prevents combo steps from
	 * advancing immediately when the attack button is pressed.
	 */
	if (ActiveAbility->GetActivationInputTag().MatchesTagExact(InputTag))
	{
		return;
	}

	/*
	 * A different ability input should be evaluated immediately while the
	 * active ability still permits early cancellation.
	 */
	if (!ActiveAbility->IsEarlyCancellationClosed() &&
		!ActiveAbility->IsCommitted() &&
		!ActiveAbility->GetAllowedEarlyCancellationAbilityTags().IsEmpty())
	{
		ResolveBufferedAbilityInput();
	}
}

void UAbilityComponent::InputReleased(FGameplayTag InputTag)
{
	if (!IsValid(InputBufferComponent) || !InputTag.IsValid())
	{
		return;
	}

	if (!InputBufferComponent->InputReleased(InputTag))
	{
		return;
	}

	if (!IsValid(ActiveAbility) || bEndingAbility)
	{
		return;
	}

	UAbility* InputAbility = ActiveAbility;

	DispatchAbilityCallback([InputAbility, InputTag]()
	{
		InputAbility->OnInputReleased(InputTag);
	});
}

void UAbilityComponent::MovementInputReceived(FVector2D MovementInput)
{
	if (IsValid(InputBufferComponent))
	{
		InputBufferComponent->SetMovementInput(MovementInput);
	}

	if (!IsValid(ActiveAbility) || bEndingAbility)
	{
		return;
	}

	UAbility* InputAbility = ActiveAbility;

	DispatchAbilityCallback([InputAbility, MovementInput]()
	{
		InputAbility->OnMovementInputReceived(MovementInput);
	});
}

FVector2D UAbilityComponent::GetMovementInput() const
{
	return IsValid(InputBufferComponent)
		? InputBufferComponent->GetMovementInput()
		: FVector2D::ZeroVector;
}
bool UAbilityComponent::IsInputHeld(FGameplayTag InputTag) const
{
	return IsValid(InputBufferComponent) && InputBufferComponent->IsInputHeld(InputTag);
}

void UAbilityComponent::ClearBufferedInputs()
{
	if (IsValid(InputBufferComponent))
	{
		InputBufferComponent->ClearBufferedInputs();
	}
}

void UAbilityComponent::HandleAbilityEvent(FGameplayTag EventTag)
{
	if (!IsValid(ActiveAbility) || !EventTag.IsValid() || bEndingAbility)
	{
		return;
	}

	UAbility* EventAbility = ActiveAbility;

	DispatchAbilityCallback([EventAbility, EventTag]()
	{
		EventAbility->OnAnimationEvent(EventTag);
	});
}

void UAbilityComponent::AddLooseOwnerTag(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}

	int32& Count = LooseOwnerTagCounts.FindOrAdd(Tag);
	const bool bWasAbsent = Count <= 0;

	++Count;

	if (bWasAbsent)
	{
		BroadcastOwnedTagsChanged();
	}
}

void UAbilityComponent::RemoveLooseOwnerTag(FGameplayTag Tag)
{
	int32* Count = LooseOwnerTagCounts.Find(Tag);

	if (!Count)
	{
		return;
	}

	if (*Count > 1)
	{
		--(*Count);
		return;
	}

	LooseOwnerTagCounts.Remove(Tag);
	BroadcastOwnedTagsChanged();
}

bool UAbilityComponent::HasOwnerTag(FGameplayTag Tag) const
{
	return Tag.IsValid() && GetOwnedGameplayTags().HasTag(Tag);
}

bool UAbilityComponent::HasAllOwnerTags(const FGameplayTagContainer& Tags) const
{
	return GetOwnedGameplayTags().HasAll(Tags);
}

bool UAbilityComponent::HasAnyOwnerTags(const FGameplayTagContainer& Tags) const
{
	return GetOwnedGameplayTags().HasAny(Tags);
}

FGameplayTagContainer UAbilityComponent::GetOwnedGameplayTags() const
{
	FGameplayTagContainer Result = BuildLooseOwnerTags();
	Result.AppendTags(ActiveGrantedTags);
	return Result;
}

bool UAbilityComponent::IsActiveAbilityCommitted() const
{
	return IsValid(ActiveAbility) && ActiveAbility->IsCommitted();
}

bool UAbilityComponent::CommitAbility(UAbility* RequestingAbility)
{
	if (!IsValid(RequestingAbility) ||
		RequestingAbility != ActiveAbility ||
		RequestingAbility->GetAbilityStatus() != EAbilityStatus::Active)
	{
		return false;
	}

	if (RequestingAbility->IsCommitted())
	{
		return true;
	}

	if (!RequestingAbility->CanCommitAbility())
	{
		return false;
	}

	RequestingAbility->SetCommitted(true);
	RequestingAbility->SetEarlyCancellationClosed(true);

	DispatchAbilityCallback([this, RequestingAbility]()
	{
		RequestingAbility->OnAbilityCommitted();
		AbilityCommittedEvent.Broadcast(RequestingAbility->GetAbilityId(), RequestingAbility);
	});

	return true;
}

void UAbilityComponent::EndAbility(UAbility* RequestingAbility, EAbilityEndReason EndReason)
{
	if (IsValid(RequestingAbility) && RequestingAbility == ActiveAbility)
	{
		EndActiveAbilityInternal(EndReason);
	}
}

void UAbilityComponent::SetAbilityTickEnabled(UAbility* RequestingAbility, bool bEnabled)
{
	if (!IsValid(RequestingAbility) || RequestingAbility != ActiveAbility || bEndingAbility)
	{
		return;
	}

	bAbilityTickEnabled = bEnabled;
	SetComponentTickEnabled(bAbilityTickEnabled);
}

void UAbilityComponent::BuildAbilityInputCandidates(TArray<FAbilityInputCandidate>& OutCandidates)
{
	OutCandidates.Reset();

	if (!IsValid(InputBufferComponent))
	{
		return;
	}

	TArray<FBufferedInput> BufferedInputs;
	InputBufferComponent->GetInputCandidates(BufferedInputs, true);

	for (const FBufferedInput& BufferedInput : BufferedInputs)
	{
		for (const TSubclassOf<UAbility> AbilityClass : GrantedAbilityClasses)
		{
			const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

			if (!IsValid(AbilityCDO) ||
				!AbilityCDO->GetActivationInputTag().IsValid() ||
				!AbilityCDO->GetActivationInputTag().MatchesTagExact(BufferedInput.InputTag))
			{
				continue;
			}

			if (BufferedInput.Source == EBufferedInputSource::Held &&
				!AbilityCDO->CanActivateFromHeldInput())
			{
				continue;
			}

			if (AbilityCDO->RequiresInputHeldAtResolution() &&
				!InputBufferComponent->IsInputHeld(BufferedInput.InputTag))
			{
				continue;
			}

			FAbilityInputCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.AbilityClass = AbilityClass;
			Candidate.BufferedInput = BufferedInput;
			Candidate.Priority = AbilityCDO->GetActivationPriority();
		}
	}

	OutCandidates.Sort([](const FAbilityInputCandidate& A, const FAbilityInputCandidate& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority > B.Priority;
		}

		return A.BufferedInput.Sequence < B.BufferedInput.Sequence;
	});
}

bool UAbilityComponent::ExecuteAbilityInputCandidate(const FAbilityInputCandidate& Candidate)
{
	if (!Candidate.AbilityClass)
	{
		return false;
	}

	if (!IsValid(ActiveAbility))
	{
		UAbility* NewAbility = CreateExecutionInstance(Candidate.AbilityClass);

		if (!IsValid(NewAbility) || !CanActivateAbilityInstance(NewAbility, false))
		{
			return false;
		}

		return ActivateAbilityInstance(NewAbility);
	}

	if (ActiveAbility->GetClass() == Candidate.AbilityClass)
	{
		// Combo abilities can consume their own input internally without
		// replacing the active execution.
		if (ActiveAbility->CanHandleRepeatedActivationRequest())
		{
			return ExecuteRepeatedAbilityRequest(ActiveAbility);
		}

		// Other abilities may transition into a fresh execution of themselves.
		return ReplaceActiveAbility(Candidate.AbilityClass);
	}

	return ReplaceActiveAbility(Candidate.AbilityClass);
}

bool UAbilityComponent::ExecuteRepeatedAbilityRequest(UAbility* Ability)
{
	if (!IsValid(Ability) ||
		Ability != ActiveAbility ||
		!Ability->CanHandleRepeatedActivationRequest())
	{
		return false;
	}

	bool bConsumed = false;

	DispatchAbilityCallback([Ability, &bConsumed]()
	{
		bConsumed = Ability->HandleRepeatedActivationRequest();
	});

	return bConsumed;
}

bool UAbilityComponent::ReplaceActiveAbility(TSubclassOf<UAbility> IncomingAbilityClass)
{
	if (!IsValid(ActiveAbility) || !IncomingAbilityClass)
	{
		return false;
	}

	UAbility* IncomingAbility = CreateExecutionInstance(IncomingAbilityClass);

	if (!IsValid(IncomingAbility))
	{
		return false;
	}

	if (!CanActivateAbilityInstance(IncomingAbility, true))
	{
		return false;
	}

	EAbilityEndReason ReplacementReason = EAbilityEndReason::Interrupted;

	if (!CanReplaceActiveAbility(ActiveAbility, IncomingAbility, ReplacementReason))
	{
		return false;
	}

	EndActiveAbilityInternal(ReplacementReason);

	if (IsValid(ActiveAbility))
	{
		return false;
	}

	return ActivateAbilityInstance(IncomingAbility);
}

bool UAbilityComponent::CanActivateAbilityInstance(const UAbility* Ability, const bool bReplacingActiveAbility) const
{
	if (!IsValid(Ability) || !Ability->GetAbilityId().IsValid())
	{
		return false;
	}

	const FGameplayTagContainer OwnerTags = bReplacingActiveAbility
		? BuildOwnedTagsWithoutActiveAbility()
		: GetOwnedGameplayTags();

	if (!OwnerTags.HasAll(Ability->GetRequiredOwnerTags()))
	{
		return false;
	}

	if (OwnerTags.HasAny(Ability->GetBlockedOwnerTags()))
	{
		return false;
	}

	return Ability->CanActivateAbility();
}

bool UAbilityComponent::CanReplaceActiveAbility(const UAbility* CurrentAbility, const UAbility* IncomingAbility, EAbilityEndReason& OutReplacementReason)
{
	OutReplacementReason = EAbilityEndReason::Interrupted;

	if (!IsValid(CurrentAbility) || !IsValid(IncomingAbility))
	{
		return false;
	}

	if (CurrentAbility->CanTransitionTo(IncomingAbility))
	{
		OutReplacementReason = EAbilityEndReason::Transitioned;
		return true;
	}

	if (CurrentAbility->CanEarlyCancelTo(IncomingAbility))
	{
		OutReplacementReason = EAbilityEndReason::EarlyCancelled;
		return true;
	}

	if (CurrentAbility->GetBlockAbilitiesWithTags().HasAny(IncomingAbility->GetAbilityTags()))
	{
		return false;
	}

	const bool bIncomingExplicitlyCancels =
		IncomingAbility->GetCancelAbilitiesWithTags().HasAny(CurrentAbility->GetAbilityTags());

	if (bIncomingExplicitlyCancels)
	{
		if (!IncomingAbility->CanReplaceActiveAbility(CurrentAbility))
		{
			return false;
		}

		OutReplacementReason = EAbilityEndReason::Interrupted;
		return true;
	}

	const bool bCanInterrupt =
		CurrentAbility->CanBeCancelledBy(IncomingAbility) &&
		IncomingAbility->CanReplaceActiveAbility(CurrentAbility);

	if (bCanInterrupt)
	{
		OutReplacementReason = EAbilityEndReason::Interrupted;
	}

	return bCanInterrupt;
}
UAbility* UAbilityComponent::CreateExecutionInstance(TSubclassOf<UAbility> AbilityClass)
{
	if (!AbilityClass || !IsValid(OwningCharacter))
	{
		return nullptr;
	}

	UAbility* NewAbility = NewObject<UAbility>(this, AbilityClass);

	if (!IsValid(NewAbility))
	{
		return nullptr;
	}

	NewAbility->InitializeAbility(this, OwningCharacter);
	return NewAbility;
}

bool UAbilityComponent::ActivateAbilityInstance(UAbility* Ability)
{
	if (!IsValid(Ability) || IsValid(ActiveAbility) || bEndingAbility)
	{
		return false;
	}

	ActiveAbility = Ability;
	ActiveAbility->SetAbilityStatus(EAbilityStatus::Active);
	ActiveAbility->SetCommitted(false);
	ActiveAbility->SetTransitionOpen(false);
	ActiveAbility->SetEarlyCancellationClosed(false);

	bAbilityTickEnabled = false;
	SetComponentTickEnabled(false);

	ApplyActiveAbilityTags();

	UAbility* ActivatedAbility = ActiveAbility;

	DispatchAbilityCallback([this, ActivatedAbility]()
	{
		AbilityActivatedEvent.Broadcast(ActivatedAbility->GetAbilityId(), ActivatedAbility);
		ActivatedAbility->ActivateAbility();
	});

	return ActiveAbility == ActivatedAbility;
}

void UAbilityComponent::EndActiveAbilityInternal(const EAbilityEndReason EndReason)
{
	if (!IsValid(ActiveAbility) || bEndingAbility)
	{
		return;
	}

	bEndingAbility = true;
	bResolveBufferedInputAfterCallback = false;

	UAbility* EndingAbility = ActiveAbility;
	const FGameplayTag EndingAbilityId = EndingAbility->GetAbilityId();

	EndingAbility->SetAbilityStatus(EAbilityStatus::Ending);
	EndingAbility->SetTransitionOpen(false);
	EndingAbility->SetEarlyCancellationClosed(true);

	bAbilityTickEnabled = false;
	SetComponentTickEnabled(false);

	RemoveActiveAbilityTags();
	

	DispatchAbilityCallback([this, EndingAbility, EndingAbilityId, EndReason]()
	{
		EndingAbility->OnAbilityEnded(EndReason);
		AbilityEndedEvent.Broadcast(EndingAbilityId, EndingAbility, EndReason);
	});

	EndingAbility->SetAbilityStatus(EAbilityStatus::Inactive);

	ActiveAbility = nullptr;
	bEndingAbility = false;

	if (!bResolvingBufferedInput && IsValid(InputBufferComponent))
	{
		ResolveBufferedAbilityInput();
	}
}
void UAbilityComponent::SetAbilityEarlyCancellationClosed(UAbility* RequestingAbility, const bool bClosed) const
{
	if (!IsValid(RequestingAbility) ||
		RequestingAbility != ActiveAbility ||
		RequestingAbility->GetAbilityStatus() != EAbilityStatus::Active ||
		bEndingAbility)
	{
		return;
	}

	RequestingAbility->SetEarlyCancellationClosed(bClosed);
}
void UAbilityComponent::DispatchAbilityCallback(const TFunctionRef<void()>& Callback)
{
	const bool bWasAlreadyDispatching = bDispatchingAbilityCallback;

	bDispatchingAbilityCallback = true;
	Callback();
	bDispatchingAbilityCallback = bWasAlreadyDispatching;

	if (bWasAlreadyDispatching)
	{
		return;
	}

	if (!bResolveBufferedInputAfterCallback ||
		bEndingAbility ||
		bResolvingBufferedInput)
	{
		return;
	}

	bResolveBufferedInputAfterCallback = false;

	const bool bResolved = ResolveBufferedAbilityInput();

	UE_LOG(LogTemp, Warning, TEXT("[Transition] Deferred resolution result=%d"), bResolved);
}

FGameplayTagContainer UAbilityComponent::BuildLooseOwnerTags() const
{
	FGameplayTagContainer Result;

	for (const TPair<FGameplayTag, int32>& Pair : LooseOwnerTagCounts)
	{
		if (Pair.Key.IsValid() && Pair.Value > 0)
		{
			Result.AddTag(Pair.Key);
		}
	}

	return Result;
}

FGameplayTagContainer UAbilityComponent::BuildOwnedTagsWithoutActiveAbility() const
{
	return BuildLooseOwnerTags();
}

void UAbilityComponent::ApplyActiveAbilityTags()
{
	ActiveGrantedTags.Reset();

	if (IsValid(ActiveAbility))
	{
		ActiveGrantedTags.AppendTags(ActiveAbility->GetGrantedOwnerTags());
	}

	BroadcastOwnedTagsChanged();
}

void UAbilityComponent::RemoveActiveAbilityTags()
{
	if (ActiveGrantedTags.IsEmpty())
	{
		return;
	}

	ActiveGrantedTags.Reset();
	BroadcastOwnedTagsChanged();
}

void UAbilityComponent::BroadcastOwnedTagsChanged() const
{
	OwnedTagsChangedEvent.Broadcast(GetOwnedGameplayTags());
}

const UAbility* UAbilityComponent::GetAbilityCDO(const TSubclassOf<UAbility> AbilityClass)
{
	return AbilityClass ? AbilityClass.GetDefaultObject() : nullptr;
}

float UAbilityComponent::GetLongestBufferDurationForInput(FGameplayTag InputTag) const
{
	float LongestDuration = 0.0f;

	for (const TSubclassOf<UAbility> AbilityClass : GrantedAbilityClasses)
	{
		const UAbility* AbilityCDO = GetAbilityCDO(AbilityClass);

		if (!IsValid(AbilityCDO) ||
			!AbilityCDO->GetActivationInputTag().MatchesTagExact(InputTag))
		{
			continue;
		}

		LongestDuration = FMath::Max(LongestDuration, AbilityCDO->GetInputBufferDuration());
	}

	return LongestDuration;
}
void UAbilityComponent::SetAbilityTransitionOpen(UAbility* RequestingAbility, bool bOpen)
{
	if (!IsValid(RequestingAbility) ||
		RequestingAbility != ActiveAbility ||
		RequestingAbility->GetAbilityStatus() != EAbilityStatus::Active ||
		bEndingAbility)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Transition] Open rejected."));
		return;
	}

	RequestingAbility->SetTransitionOpen(bOpen);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Transition] %s | Ability=%s | Dispatching=%d | Resolving=%d"),
		bOpen ? TEXT("OPENED") : TEXT("CLOSED"),
		*GetNameSafe(RequestingAbility),
		bDispatchingAbilityCallback,
		bResolvingBufferedInput
	);

	if (!bOpen)
	{
		return;
	}

	if (bDispatchingAbilityCallback)
	{
		bResolveBufferedInputAfterCallback = true;
		UE_LOG(LogTemp, Warning, TEXT("[Transition] Resolution deferred until callback returns."));
		return;
	}

	if (!bResolvingBufferedInput)
	{
		const bool bResolved = ResolveBufferedAbilityInput();
		UE_LOG(LogTemp, Warning, TEXT("[Transition] Immediate resolution result=%d"), bResolved);
	}
}