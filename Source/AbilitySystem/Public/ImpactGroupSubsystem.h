#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ImpactGroupTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "ImpactGroupSubsystem.generated.h"

USTRUCT()
struct FImpactGroupRecord
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = INDEX_NONE;

	UPROPERTY()
	FImpactGroupSettings Settings;

	int32 OutstandingMembers = 0;
	bool bSealed = false;

	double OpenedAt = 0.0;
	double LastHitPlayedAt = -1.0;
	double LastMissPlayedAt = -1.0;
};

class AActor;
class UFeedbackSet;
class UPrimitiveComponent;
struct FFeedbackCueContext;
struct FFeedbackEffect;

/** Everything one impact needs to play its feedback. */
struct FImpactReport
{
	FImpactGroupHandle Group;
	EImpactResult Result = EImpactResult::Miss;

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;

	/** The hitter's look (weapon or projectile set). Null = the project's default set only. */
	const UFeedbackSet* FeedbackSet = nullptr;

	/** What FeedbackSet sits on when it is an override (the arrow's own set under an ability's look). */
	const UFeedbackSet* FallbackSet = nullptr;

	/** Played for Hit and Miss results (Cue.Projectile.Hit / Cue.Projectile.Miss). */
	FGameplayTag HitCue;
	FGameplayTag MissCue;

	AActor* Instigator = nullptr;
	AActor* Target = nullptr;

	/** 0..1 how hard the hit was; sent to the effects as the "Strength" Niagara parameter. Below 0 = not sent. */
	float Strength = -1.0f;

	/** Surface type and normal. */
	FHitResult Hit;
	const UPrimitiveComponent* HitComponent = nullptr;
};

/**
 * Merges the impact feedback of hits that belong to one ability use (a volley, a cleave).
 * Lives on the world so groups outlive both the ability and the weapon.
 *
 * Every offensive ability opens one when it activates and seals it when it ends;
 * projectiles it fired join as members and keep the group alive until they land.
 * A sealed group with no outstanding members is removed; stale groups expire.
 * What plays comes from the hitter's Feedback Set through the Feedback Subsystem.
 */
UCLASS()
class ABILITYSYSTEM_API UImpactGroupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	
	/** Plays an impact's own feedback and, if grouped, the group's feedback per its settings. */
	void PlayImpact(const FImpactReport& Report);

	static UImpactGroupSubsystem* Get(const UObject* WorldContextObject);

	FImpactGroupHandle OpenGroup(const FImpactGroupSettings& Settings);

	/** False if the group is gone or sealed; the member should then behave ungrouped. */
	bool AddMember(const FImpactGroupHandle& Handle);

	void ResolveMember(const FImpactGroupHandle& Handle);

	/** No more members will join. Sealing an empty group removes it immediately. */
	void SealGroup(const FImpactGroupHandle& Handle);

	/** Debug aid: open groups right now. Should return to 0 after every volley settles. */
	int32 GetOpenGroupCount() const { return Groups.Num(); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	int32 FindGroupIndex(const FImpactGroupHandle& Handle) const;
	void RemoveIfFinished(int32 GroupIndex);
	void PruneExpiredGroups();
	double GetTimeSeconds() const;

	/** Safety net for members that never resolve. */
	static constexpr double MaxGroupLifetime = 30.0;

	UPROPERTY(Transient)
	TArray<FImpactGroupRecord> Groups;

	int32 NextGroupId = 0;
	
	/** Advances Policy for Result; true if the group layer should play for this impact. */
	bool ConsumeGroupSlot(FImpactGroupRecord& Record, EImpactResult Result) const;

	/** The impact's own feedback: its set's hit or miss cue for this surface. */
	FFeedbackEffect ResolveOwnEffect(const FImpactReport& Report, const FFeedbackCueContext& Context) const;

	/** The group's shared Cue.Group.Hit / Cue.Group.Miss from the impact's set. Empty when none is defined. */
	FFeedbackEffect ResolveGroupEffect(const FImpactGroupRecord& Record, const FImpactReport& Report, const FFeedbackCueContext& Context) const;
};