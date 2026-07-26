#include "InputBufferComponent.h"

#include "Engine/World.h"

UInputBufferComponent::UInputBufferComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UInputBufferComponent::SetMovementInput(const FVector2D MovementInput)
{
	CurrentMovementInput = MovementInput.GetClampedToMaxSize(1.0f);
}

void UInputBufferComponent::BeginPlay()
{
	Super::BeginPlay();

	BufferedInputs.Reset();
	HeldInputTags.Reset();
	HeldInputSequences.Reset();
	NextSequence = 1;
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
	return BufferDurationOverride >= 0.0f
		? BufferDurationOverride
		: DefaultBufferDuration;
}

bool UInputBufferComponent::HasPressedEntry(FGameplayTag InputTag) const
{
	return BufferedInputs.ContainsByPredicate([InputTag](const FBufferedInput& Entry)
	{
		return Entry.Source == EBufferedInputSource::Pressed &&
			Entry.InputTag.MatchesTagExact(InputTag);
	});
}