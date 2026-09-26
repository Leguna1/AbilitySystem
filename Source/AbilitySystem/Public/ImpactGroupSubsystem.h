#pragma once

#include "CoreMinimal.h"
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

class UPrimitiveComponent;

/** Everything one impact needs to play its feedback. */
struct FImpactReport
{
	FImpactGroupHandle Group;
	EImpactResult Result = EImpactResult::Miss;

	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;

	/** The hitter's own hit feedback (arrow data, melee ability). Misses use the environment. */
	FImpactFeedback OwnHitFeedback;

	/** For surface resolution (stage 2). */
	FHitResult Hit;
	const UPrimitiveComponent* HitComponent = nullptr;
};

/**
 * Merges impact audio for hits that belong to one action (a volley, a swing).
 * Lives on the world so groups outlive both the ability and the weapon.
 *
 * Lifecycle: OpenGroup -> AddMember per projectile -> SealGroup once no more
 * members will join -> ResolveMember as each projectile impacts or recycles.
 * A sealed group with no outstanding members is removed; stale groups expire.
 */
UCLASS()
class ABILITYSYSTEM_API UImpactGroupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Plays an impact's own feedback and, if grouped, the group's feedback per its settings. */
	void PlayImpact(const FImpactReport& Report);

	/** Feedback for a miss on this surface. Stage 1: always the environment default. */
	FImpactFeedback ResolveEnvironmentImpact(const FHitResult& Hit, const UPrimitiveComponent* HitComponent) const;
	
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

	void PlayFeedback(const FImpactFeedback& Feedback, const FImpactReport& Report, bool bPlaySound, bool bPlayEffect) const;

	/** Loaded from UAbilitySystemSettings when the world starts. */
	UPROPERTY(Transient)
	FImpactFeedback EnvironmentImpact;
};