#include "ImpactGroupSubsystem.h"

#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

#include "Engine/World.h"
#include "FeedbackSet.h"
#include "FeedbackSubsystem.h"
#include "FeedbackTags.h"
#include "FeedbackTypes.h"

namespace
{
	FFeedbackCueContext MakeContext(const FImpactReport& Report)
	{
		FFeedbackCueContext Context;
		Context.Location = Report.Location;
		Context.Rotation = Report.Rotation;
		Context.Hit = Report.Hit;
		Context.HitComponent = Report.HitComponent;
		Context.Instigator = Report.Instigator;
		Context.Target = Report.Target;

		if (Report.Strength >= 0.0f)
		{
			Context.NiagaraFloats.Add(TEXT("Strength"), Report.Strength);
		}

		return Context;
	}
}

UImpactGroupSubsystem* UImpactGroupSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	return IsValid(World) ? World->GetSubsystem<UImpactGroupSubsystem>() : nullptr;
}

bool UImpactGroupSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

FImpactGroupHandle UImpactGroupSubsystem::OpenGroup(const FImpactGroupSettings& Settings)
{
	PruneExpiredGroups();

	FImpactGroupRecord& Record = Groups.AddDefaulted_GetRef();
	Record.Id = NextGroupId++;
	Record.Settings = Settings;
	Record.OpenedAt = GetTimeSeconds();

	FImpactGroupHandle Handle;
	Handle.Id = Record.Id;
	return Handle;
}

bool UImpactGroupSubsystem::AddMember(const FImpactGroupHandle& Handle)
{
	const int32 GroupIndex = FindGroupIndex(Handle);

	if (GroupIndex == INDEX_NONE || Groups[GroupIndex].bSealed)
	{
		return false;
	}

	++Groups[GroupIndex].OutstandingMembers;
	return true;
}

void UImpactGroupSubsystem::ResolveMember(const FImpactGroupHandle& Handle)
{
	const int32 GroupIndex = FindGroupIndex(Handle);

	if (GroupIndex == INDEX_NONE)
	{
		return;
	}

	FImpactGroupRecord& Record = Groups[GroupIndex];
	Record.OutstandingMembers = FMath::Max(0, Record.OutstandingMembers - 1);

	RemoveIfFinished(GroupIndex);
}

void UImpactGroupSubsystem::SealGroup(const FImpactGroupHandle& Handle)
{
	const int32 GroupIndex = FindGroupIndex(Handle);

	if (GroupIndex == INDEX_NONE)
	{
		return;
	}

	Groups[GroupIndex].bSealed = true;
	RemoveIfFinished(GroupIndex);
}

FFeedbackEffect UImpactGroupSubsystem::ResolveOwnEffect(const FImpactReport& Report, const FFeedbackCueContext& Context) const
{
	const bool bHit = Report.Result == EImpactResult::Hit;

	FFeedbackEffect Effect;
	const UFeedbackSubsystem* Feedback = UFeedbackSubsystem::Get(this);

	if (IsValid(Feedback))
	{
		Feedback->ResolveEffect(Report.FeedbackSet, bHit ? Report.HitCue : Report.MissCue, Context, Effect, Report.FallbackSet);
	}

	return Effect;
}

FFeedbackEffect UImpactGroupSubsystem::ResolveGroupEffect(const FImpactGroupRecord& Record, const FImpactReport& Report, const FFeedbackCueContext& Context) const
{
	const bool bHit = Report.Result == EImpactResult::Hit;
	const UFeedbackSubsystem* Feedback = UFeedbackSubsystem::Get(this);

	FFeedbackEffect Effect;

	if (IsValid(Feedback))
	{
		Feedback->ResolveEffect(Report.FeedbackSet, bHit ? FeedbackTags::Cue_Group_Hit : FeedbackTags::Cue_Group_Miss, Context, Effect, Report.FallbackSet);
	}

	return Effect;
}

void UImpactGroupSubsystem::PlayImpact(const FImpactReport& Report)
{
	UFeedbackSubsystem* Feedback = UFeedbackSubsystem::Get(this);

	if (!IsValid(Feedback))
	{
		return;
	}

	const bool bHit = Report.Result == EImpactResult::Hit;
	const FFeedbackCueContext Context = MakeContext(Report);

	const FFeedbackEffect OwnEffect = ResolveOwnEffect(Report, Context);
	const FGameplayTag OwnCue = bHit ? Report.HitCue : Report.MissCue;

	const int32 GroupIndex = FindGroupIndex(Report.Group);

	// Ungrouped, or the group already expired: full per-impact feedback.
	if (GroupIndex == INDEX_NONE)
	{
		Feedback->PlayEffect(OwnEffect, Context, OwnCue);
		return;
	}

	FImpactGroupRecord& Record = Groups[GroupIndex];

	const bool bOwnSound = Record.Settings.EachHit == EImpactHitFeedback::SoundAndEffect;
	const bool bOwnEffect = Record.Settings.EachHit != EImpactHitFeedback::None;

	Feedback->PlayEffect(OwnEffect, Context, OwnCue, bOwnSound, bOwnEffect);

	if (!ConsumeGroupSlot(Record, Report.Result))
	{
		return;
	}

	// The group already decided how often it plays; no per-frame limit on top.
	const FFeedbackEffect GroupEffect = ResolveGroupEffect(Record, Report, Context);
	Feedback->PlayEffect(GroupEffect, Context, FGameplayTag());

	// No group sound for this result: the group voices the impact's own sound instead.
	if (!GroupEffect.HasSound() && !bOwnSound)
	{
		Feedback->PlayEffect(OwnEffect, Context, FGameplayTag(), true, false);
	}
}

bool UImpactGroupSubsystem::ConsumeGroupSlot(FImpactGroupRecord& Record, const EImpactResult Result) const
{
	double& LastPlayedAt = Result == EImpactResult::Hit
		? Record.LastHitPlayedAt
		: Record.LastMissPlayedAt;

	const double Now = GetTimeSeconds();
	bool bPlay = false;

	switch (Record.Settings.Policy)
	{
	case EImpactGroupAudio::PerImpact:
		bPlay = true;
		break;

	case EImpactGroupAudio::OncePerWindow:
		bPlay = LastPlayedAt < 0.0 || Now - LastPlayedAt >= Record.Settings.Window;
		break;

	case EImpactGroupAudio::OncePerGroup:
	default:
		bPlay = LastPlayedAt < 0.0;
		break;
	}

	if (bPlay)
	{
		LastPlayedAt = Now;
	}

	return bPlay;
}

int32 UImpactGroupSubsystem::FindGroupIndex(const FImpactGroupHandle& Handle) const
{
	if (!Handle.IsValid())
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < Groups.Num(); ++Index)
	{
		if (Groups[Index].Id == Handle.Id)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void UImpactGroupSubsystem::RemoveIfFinished(const int32 GroupIndex)
{
	if (Groups.IsValidIndex(GroupIndex) &&
		Groups[GroupIndex].bSealed &&
		Groups[GroupIndex].OutstandingMembers <= 0)
	{
		Groups.RemoveAtSwap(GroupIndex);
	}
}

void UImpactGroupSubsystem::PruneExpiredGroups()
{
	const double Now = GetTimeSeconds();

	for (int32 Index = Groups.Num() - 1; Index >= 0; --Index)
	{
		if (Now - Groups[Index].OpenedAt > MaxGroupLifetime)
		{
			Groups.RemoveAtSwap(Index);
		}
	}
}

double UImpactGroupSubsystem::GetTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}
