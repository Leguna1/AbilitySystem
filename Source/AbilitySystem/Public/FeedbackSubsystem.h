// FeedbackSubsystem.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "FeedbackTypes.h"
#include "FeedbackSubsystem.generated.h"

class UFeedbackSet;
class UPrimitiveComponent;

/**
 * Plays one-shot cues (hits, launches, swings, draws) for anything in the world.
 * Weapons and projectiles report a cue tag plus their Feedback Set; this finds the
 * entry (set -> parents -> project default), picks the surface variant, and plays it
 * with pooled Niagara, per-frame limits and distance culling.
 *
 * Loops (auras, trails, flight) are owned by a UFeedbackComponent on the actor they follow.
 * When an impact belongs to a group (a volley), UImpactGroupSubsystem decides what plays and calls in here.
 */
UCLASS()
class ABILITYSYSTEM_API UFeedbackSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UFeedbackSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Finds Tag in Set's chain, then FallbackSet's chain, then the project's default set.
	 * FallbackSet is what an override sits on (an arrow's own set under an ability's look).
	 * Null = nothing defined (plays nothing).
	 */
	const FFeedbackCue* FindCue(const UFeedbackSet* Set, const FGameplayTag& Tag, const UFeedbackSet* FallbackSet = nullptr) const;

	/** Finds and plays Tag. False if no set defines it or it was limited/culled. */
	bool PlayCue(const UFeedbackSet* Set, const FGameplayTag& Tag, const FFeedbackCueContext& Context, const UFeedbackSet* FallbackSet = nullptr);

	/**
	 * Plays an already-resolved effect. CueForLimits counts toward the per-frame limit
	 * (leave empty to skip the limit). Sound and effect can be played separately.
	 */
	bool PlayEffect(const FFeedbackEffect& Effect, const FFeedbackCueContext& Context, const FGameplayTag& CueForLimits, bool bPlaySound = true, bool bPlayEffect = true);

	/** Resolves Tag for this hit's surface. False if nothing defines it. */
	bool ResolveEffect(const UFeedbackSet* Set, const FGameplayTag& Tag, const FFeedbackCueContext& Context, FFeedbackEffect& OutEffect, const UFeedbackSet* FallbackSet = nullptr) const;

	/** Surface type of a hit: its physical material, else the struck component's. Default when unknown. */
	static EPhysicalSurface ResolveSurface(const FHitResult& Hit, const UPrimitiveComponent* HitComponent);

	/**
	 * Where a strike landed: the hit's impact point, else the point on HitActor's collision
	 * nearest StrikeOrigin (overlaps don't report one). Call before delivering a payload that may kill.
	 */
	static FVector ResolveStrikeLocation(const AActor* HitActor, const FHitResult& Hit, const FVector& StrikeOrigin);

	const UFeedbackSet* GetDefaultSet() const { return DefaultSet; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** True if Tag may still play this frame; counts it. */
	bool ConsumeFrameBudget(const FGameplayTag& Tag);

	/** True when Location is farther than the cull distance from every local player's camera. */
	bool IsCulled(const FVector& Location) const;

	FRotator ResolveRotation(const FFeedbackEffect& Effect, const FFeedbackCueContext& Context) const;

	UPROPERTY(Transient)
	TObjectPtr<const UFeedbackSet> DefaultSet;

	TMap<FGameplayTag, int32> PlaysThisFrame;
	uint64 CountedFrame = 0;

	int32 MaxSameCuePerFrame = 4;
	float EffectCullDistanceSquared = 0.0f;
};
