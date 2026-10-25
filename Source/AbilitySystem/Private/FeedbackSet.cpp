// FeedbackSet.cpp
#include "FeedbackSet.h"

#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include "FeedbackTags.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	/** Guards against a parent loop (A -> B -> A) that slipped past validation. */
	constexpr int32 MaxParentDepth = 16;
}

const FFeedbackCue* UFeedbackSet::FindCue(const FGameplayTag& Tag) const
{
	return FindInChain(this, Tag);
}

const FFeedbackCue* UFeedbackSet::FindInChain(const UFeedbackSet* Set, const FGameplayTag& Tag)
{
	if (!Tag.IsValid())
	{
		return nullptr;
	}

	int32 Depth = 0;

	for (const UFeedbackSet* Current = Set; IsValid(Current) && Depth < MaxParentDepth; Current = Current->Parent, ++Depth)
	{
		for (FGameplayTag Level = Tag; Level.IsValid(); Level = Level.RequestDirectParent())
		{
			if (const FFeedbackCue* Cue = Current->FindOwnCue(Level))
			{
				return Cue;
			}
		}
	}

	return nullptr;
}

#if WITH_EDITOR
EDataValidationResult UFeedbackSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Parent loop.
	int32 Depth = 0;

	for (const UFeedbackSet* Current = Parent; IsValid(Current); Current = Current->Parent)
	{
		if (Current == this || ++Depth >= MaxParentDepth)
		{
			Context.AddError(NSLOCTEXT("FeedbackSet", "ParentLoop", "Parent chain loops back to this set. Break the loop."));
			Result = EDataValidationResult::Invalid;
			break;
		}
	}

	for (const TPair<FGameplayTag, FFeedbackCue>& Pair : Cues)
	{
		const FText TagText = FText::FromString(Pair.Key.ToString());

		if (!Pair.Key.IsValid())
		{
			Context.AddWarning(NSLOCTEXT("FeedbackSet", "NoTag", "A cue has no tag and will never play."));
			continue;
		}

		const FFeedbackCue& Cue = Pair.Value;

		if (Cue.Effect.IsEmpty() && Cue.SurfaceVariants.IsEmpty())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("FeedbackSet", "Empty", "{0} plays nothing. That silences it (parents are not used); remove it to inherit instead."),
				TagText));
		}

		if (FeedbackTags::IsLoopCue(Pair.Key))
		{
			if (IsValid(Cue.Effect.Sound) && !Cue.Effect.Sound->IsLooping())
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("FeedbackSet", "NotLooping", "{0} is a loop, but its sound doesn't loop, so it will go quiet while the loop is still running."),
					TagText));
			}

			if (!Cue.SurfaceVariants.IsEmpty())
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("FeedbackSet", "LoopSurfaces", "{0} is a loop; surface variants only apply to hits and are ignored."),
					TagText));
			}
		}
	}

	return Result;
}
#endif
