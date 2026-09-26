#include "ImpactGroupSubsystem.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "AbilitySystemSettings.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

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

void UImpactGroupSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Loaded once at world start so the first miss never hitches.
	const UAbilitySystemSettings* Settings = GetDefault<UAbilitySystemSettings>();

	EnvironmentImpact.Sound = Settings->EnvironmentImpactSound.LoadSynchronous();
	EnvironmentImpact.Effect = Settings->EnvironmentImpactEffect.LoadSynchronous();
	EnvironmentImpact.PitchMin = Settings->EnvironmentPitchMin;
	EnvironmentImpact.PitchMax = Settings->EnvironmentPitchMax;
}

FImpactFeedback UImpactGroupSubsystem::ResolveEnvironmentImpact(const FHitResult& Hit, const UPrimitiveComponent* HitComponent) const
{
	// Stage 2 will look up the surface type here; callers stay unchanged.
	return EnvironmentImpact;
}

void UImpactGroupSubsystem::PlayImpact(const FImpactReport& Report)
{
	const bool bHit = Report.Result == EImpactResult::Hit;

	const FImpactFeedback OwnFeedback = bHit
		? Report.OwnHitFeedback
		: ResolveEnvironmentImpact(Report.Hit, Report.HitComponent);

	const int32 GroupIndex = FindGroupIndex(Report.Group);

	// Ungrouped, or the group already expired: full per-impact feedback.
	if (GroupIndex == INDEX_NONE)
	{
		PlayFeedback(OwnFeedback, Report, true, true);
		return;
	}

	FImpactGroupRecord& Record = Groups[GroupIndex];

	const bool bOwnSound = Record.Settings.ProjectileFeedback == EProjectileImpactFeedback::SoundAndEffect;
	const bool bOwnEffect = Record.Settings.ProjectileFeedback != EProjectileImpactFeedback::None;

	PlayFeedback(OwnFeedback, Report, bOwnSound, bOwnEffect);

	if (!ConsumeGroupSlot(Record, Report.Result))
	{
		return;
	}

	const FImpactFeedback& GroupFeedback = bHit ? Record.Settings.GroupHit : Record.Settings.GroupMiss;
	PlayFeedback(GroupFeedback, Report, true, true);

	// No group sound for this result: the group voices the impact's own sound instead.
	if (!IsValid(GroupFeedback.Sound) && !bOwnSound)
	{
		PlayFeedback(OwnFeedback, Report, true, false);
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

void UImpactGroupSubsystem::PlayFeedback(const FImpactFeedback& Feedback, const FImpactReport& Report, const bool bPlaySound, const bool bPlayEffect) const
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	if (bPlaySound && IsValid(Feedback.Sound))
	{
		const float Pitch = FMath::FRandRange(
			FMath::Min(Feedback.PitchMin, Feedback.PitchMax),
			FMath::Max(Feedback.PitchMin, Feedback.PitchMax)
		);

		UGameplayStatics::PlaySoundAtLocation(World, Feedback.Sound, Report.Location, 1.0f, Pitch);
	}

	if (bPlayEffect && IsValid(Feedback.Effect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Feedback.Effect, Report.Location, Report.Rotation);
	}
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
