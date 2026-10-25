// FeedbackLibrary.cpp
#include "FeedbackLibrary.h"

#include "FeedbackSubsystem.h"

bool UFeedbackLibrary::PlayFeedbackCue(const UObject* WorldContextObject, const UFeedbackSet* Set, const FGameplayTag Cue, const FVector Location, const FRotator Rotation, const FHitResult& Hit)
{
	UFeedbackSubsystem* Subsystem = UFeedbackSubsystem::Get(WorldContextObject);

	if (!IsValid(Subsystem))
	{
		return false;
	}

	FFeedbackCueContext Context;
	Context.Location = Location;
	Context.Rotation = Rotation;
	Context.Hit = Hit;

	return Subsystem->PlayCue(Set, Cue, Context);
}
