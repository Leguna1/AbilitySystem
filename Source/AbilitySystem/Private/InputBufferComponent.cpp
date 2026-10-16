#include "InputBufferComponent.h"

#include "AbilityComponent.h"
#include "ActiveAbility.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

UInputBufferComponent::UInputBufferComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInputBufferComponent::BeginPlay()
{
	Super::BeginPlay();

	BufferedInputs.Reset();
	HeldInputTags.Reset();
	HeldInputSequences.Reset();
	NextSequence = 1;

	AbilityComponent = GetOwner()->FindComponentByClass<UAbilityComponent>();

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->ActivationWindowEvent.AddDynamic(this, &UInputBufferComponent::HandleActivationWindowEvent);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UInputBufferComponent on %s found no UAbilityComponent: input won't start abilities."),
			*GetNameSafe(GetOwner()));
	}
}

void UInputBufferComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->ActivationWindowEvent.RemoveDynamic(this, &UInputBufferComponent::HandleActivationWindowEvent);
	}

	Super::EndPlay(EndPlayReason);
}

/* ============================== Entry points ============================== */

void UInputBufferComponent::HandleInput(const FGameplayTag InputTag, const bool bPressed)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	if (!bPressed)
	{
		if (InputReleased(InputTag) && IsValid(AbilityComponent))
		{
			AbilityComponent->NotifyInputReleased(InputTag);
		}
		return;
	}

	if (!InputPressed(InputTag, GetLongestBufferDurationForInput(InputTag)) || !IsValid(AbilityComponent))
	{
		return;
	}

	// The active ability hears the press first (charging, aiming, its own combo logic).
	AbilityComponent->NotifyInputPressed(InputTag);

	if (ShouldResolveOnPress(InputTag))
	{
		ResolveBufferedInputs();
	}
}

void UInputBufferComponent::HandleMovementInput(const FVector2D MovementInput)
{
	SetMovementInput(MovementInput);

	if (IsValid(AbilityComponent))
	{
		AbilityComponent->MovementInputReceived(CurrentMovementInput);
	}
}

bool UInputBufferComponent::ResolveBufferedInputs()
{
	if (!IsValid(AbilityComponent) || bResolving)
	{
		return false;
	}

	bResolving = true;
	ON_SCOPE_EXIT { bResolving = false; };

	TArray<FInputActivationCandidate> Candidates;
	BuildActivationCandidates(Candidates);

	for (const FInputActivationCandidate& Candidate : Candidates)
	{
		if (AbilityComponent->TryActivateAbility(Candidate.AbilityId))
		{
			ConsumeInput(Candidate.BufferedInput);
			return true;
		}
	}

	return false;
}

void UInputBufferComponent::HandleActivationWindowEvent(const EActivationWindowEvent WindowEvent)
{
	switch (WindowEvent)
	{
	case EActivationWindowEvent::Opened:
		ResolveBufferedInputs();
		break;

	case EActivationWindowEvent::Flushed:
		ClearBufferedInputs();
		break;
	}
}

bool UInputBufferComponent::ShouldResolveOnPress(const FGameplayTag InputTag) const
{
	const UActiveAbility* CurrentAbility = AbilityComponent->GetActiveAbility();

	if (!IsValid(CurrentAbility))
	{
		return true;
	}

	// The active ability's own input waits for its checkpoint or transition,
	// so a combo doesn't advance the instant the button is pressed.
	if (CurrentAbility->GetActivationInputTag().MatchesTagExact(InputTag))
	{
		return false;
	}

	// Another ability's input goes through right away while the active one can still be early-cancelled.
	return !CurrentAbility->IsEarlyCancellationClosed() &&
		!CurrentAbility->IsCommitted() &&
		!CurrentAbility->GetAllowedEarlyCancellationAbilityTags().IsEmpty();
}

void UInputBufferComponent::BuildActivationCandidates(TArray<FInputActivationCandidate>& OutCandidates)
{
	OutCandidates.Reset();

	TArray<FBufferedInput> WaitingInputs;
	GetInputCandidates(WaitingInputs, true);

	for (const FBufferedInput& WaitingInput : WaitingInputs)
	{
		for (const TSubclassOf<UAbility> AbilityClass : AbilityComponent->GetGrantedAbilityClasses())
		{
			// Passives fail the cast and never become candidates.
			const UActiveAbility* AbilityDefaults = Cast<UActiveAbility>(AbilityComponent->GetAbilityDefaults(AbilityClass));

			if (!IsValid(AbilityDefaults) ||
				!AbilityComponent->GetAbilityInputTag(AbilityDefaults->GetAbilityId()).IsValid() ||
				!AbilityComponent->GetAbilityInputTag(AbilityDefaults->GetAbilityId()).MatchesTagExact(WaitingInput.InputTag) ||
				AbilityComponent->GetAbilityRank(AbilityClass) <= 0)
			{
				continue;
			}

			if (WaitingInput.Source == EBufferedInputSource::Held && !AbilityDefaults->CanActivateFromHeldInput())
			{
				continue;
			}

			if (AbilityDefaults->RequiresInputHeldAtResolution() && !IsInputHeld(WaitingInput.InputTag))
			{
				continue;
			}

			FInputActivationCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.AbilityId = AbilityDefaults->GetAbilityId();
			Candidate.BufferedInput = WaitingInput;
			Candidate.Priority = AbilityDefaults->GetActivationPriority();
		}
	}

	OutCandidates.Sort([](const FInputActivationCandidate& A, const FInputActivationCandidate& B)
	{
		if (A.Priority != B.Priority)
		{
			return A.Priority > B.Priority;
		}

		return A.BufferedInput.Sequence < B.BufferedInput.Sequence;
	});
}

float UInputBufferComponent::GetLongestBufferDurationForInput(const FGameplayTag InputTag) const
{
	if (!IsValid(AbilityComponent))
	{
		return -1.0f;
	}

	float LongestDuration = 0.0f;
	bool bFoundAbility = false;

	for (const TSubclassOf<UAbility> AbilityClass : AbilityComponent->GetGrantedAbilityClasses())
	{
		const UActiveAbility* AbilityDefaults = Cast<UActiveAbility>(AbilityComponent->GetAbilityDefaults(AbilityClass));

		if (!IsValid(AbilityDefaults) || !AbilityComponent->GetAbilityInputTag(AbilityDefaults->GetAbilityId()).MatchesTagExact(InputTag))
		{
			continue;
		}

		LongestDuration = FMath::Max(LongestDuration, AbilityDefaults->GetInputBufferDuration());
		bFoundAbility = true;
	}

	// No ability on this input: fall back to the default duration.
	return bFoundAbility ? LongestDuration : -1.0f;
}

/* ============================== Storage ============================== */

void UInputBufferComponent::SetMovementInput(const FVector2D MovementInput)
{
	CurrentMovementInput = MovementInput.GetClampedToMaxSize(1.0f);
}

bool UInputBufferComponent::InputPressed(FGameplayTag InputTag, float BufferDurationOverride)
{
	if (!InputTag.IsValid() || HeldInputTags.HasTagExact(InputTag))
	{
		return false;
	}

	const float CurrentTime = GetCurrentTime();
	const float BufferDuration = ResolveBufferDuration(BufferDurationOverride);
	const uint64 Sequence = NextSequence++;

	HeldInputTags.AddTag(InputTag);
	HeldInputSequences.Add(InputTag, Sequence);

	BufferedInputs.RemoveAll([InputTag](const FBufferedInput& Entry)
	{
		return Entry.InputTag.MatchesTagExact(InputTag);
	});

	FBufferedInput& NewEntry = BufferedInputs.AddDefaulted_GetRef();
	NewEntry.InputTag = InputTag;
	NewEntry.Source = EBufferedInputSource::Pressed;
	NewEntry.BufferedAtTime = CurrentTime;
	NewEntry.ExpiresAtTime = CurrentTime + BufferDuration;
	NewEntry.Sequence = Sequence;

	InputPressedEvent.Broadcast(InputTag);
	return true;
}

bool UInputBufferComponent::InputReleased(FGameplayTag InputTag)
{
	if (!InputTag.IsValid() || !HeldInputTags.HasTagExact(InputTag))
	{
		return false;
	}

	HeldInputTags.RemoveTag(InputTag);
	HeldInputSequences.Remove(InputTag);

	InputReleasedEvent.Broadcast(InputTag);
	return true;
}

bool UInputBufferComponent::IsInputHeld(FGameplayTag InputTag) const
{
	return InputTag.IsValid() && HeldInputTags.HasTagExact(InputTag);
}

bool UInputBufferComponent::HasBufferedInput(FGameplayTag InputTag) const
{
	if (!InputTag.IsValid())
	{
		return false;
	}

	const float CurrentTime = GetCurrentTime();

	return BufferedInputs.ContainsByPredicate([InputTag, CurrentTime](const FBufferedInput& Entry)
	{
		return Entry.InputTag.MatchesTagExact(InputTag) && !Entry.IsExpired(CurrentTime);
	});
}

void UInputBufferComponent::GetInputCandidates(TArray<FBufferedInput>& OutCandidates, bool bIncludeHeldInputs)
{
	PruneExpiredInputs();

	OutCandidates = BufferedInputs;

	if (!bIncludeHeldInputs)
	{
		return;
	}

	for (const FGameplayTag& HeldTag : HeldInputTags)
	{
		if (!HeldTag.IsValid() || HasPressedEntry(HeldTag))
		{
			continue;
		}

		const uint64* HeldSequence = HeldInputSequences.Find(HeldTag);

		if (!HeldSequence)
		{
			continue;
		}

		FBufferedInput& HeldEntry = OutCandidates.AddDefaulted_GetRef();
		HeldEntry.InputTag = HeldTag;
		HeldEntry.Source = EBufferedInputSource::Held;
		HeldEntry.BufferedAtTime = GetCurrentTime();
		HeldEntry.ExpiresAtTime = TNumericLimits<float>::Max();
		HeldEntry.Sequence = *HeldSequence;
	}

	OutCandidates.Sort([](const FBufferedInput& A, const FBufferedInput& B)
	{
		return A.Sequence < B.Sequence;
	});
}

bool UInputBufferComponent::ConsumeInput(const FBufferedInput& Input)
{
	if (!Input.InputTag.IsValid())
	{
		return false;
	}

	if (Input.Source == EBufferedInputSource::Held)
	{
		return IsInputHeld(Input.InputTag);
	}

	const int32 RemovedCount = BufferedInputs.RemoveAll([&Input](const FBufferedInput& Entry)
	{
		return Entry.Source == EBufferedInputSource::Pressed &&
			Entry.Sequence == Input.Sequence &&
			Entry.InputTag.MatchesTagExact(Input.InputTag);
	});

	return RemovedCount > 0;
}

void UInputBufferComponent::ClearBufferedInput(FGameplayTag InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	BufferedInputs.RemoveAll([InputTag](const FBufferedInput& Entry)
	{
		return Entry.InputTag.MatchesTagExact(InputTag);
	});
}

void UInputBufferComponent::ClearBufferedInputs()
{
	if (BufferedInputs.IsEmpty())
	{
		return;
	}

	BufferedInputs.Reset();
	InputBufferClearedEvent.Broadcast();
}

void UInputBufferComponent::ResetInputBuffer()
{
	BufferedInputs.Reset();
	HeldInputTags.Reset();
	HeldInputSequences.Reset();
	CurrentMovementInput = FVector2D::ZeroVector;
	NextSequence = 1;

	InputBufferClearedEvent.Broadcast();
}

void UInputBufferComponent::PruneExpiredInputs()
{
	const float CurrentTime = GetCurrentTime();

	BufferedInputs.RemoveAll([CurrentTime](const FBufferedInput& Entry)
	{
		return Entry.IsExpired(CurrentTime);
	});
}

float UInputBufferComponent::GetCurrentTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0f;
}

float UInputBufferComponent::ResolveBufferDuration(float BufferDurationOverride) const
{
	return BufferDurationOverride >= 0.0f ? BufferDurationOverride : DefaultBufferDuration;
}

bool UInputBufferComponent::HasPressedEntry(FGameplayTag InputTag) const
{
	return BufferedInputs.ContainsByPredicate([InputTag](const FBufferedInput& Entry)
	{
		return Entry.Source == EBufferedInputSource::Pressed &&
			Entry.InputTag.MatchesTagExact(InputTag);
	});
}
