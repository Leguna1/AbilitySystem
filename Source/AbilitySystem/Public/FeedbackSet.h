// FeedbackSet.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FeedbackTypes.h"
#include "FeedbackSet.generated.h"

/**
 * What a weapon, projectile or character looks and sounds like, as cue tag -> effect.
 * Weapons and projectiles only say "Cue.Projectile.Hit happened"; their set decides what plays.
 *
 * Parent: cues this set doesn't have come from its parent (DA_Feedback_FireArrow only
 * overrides Flight and Hit, everything else comes from DA_Feedback_Arrow). The Default
 * Feedback Set in Project Settings -> Game -> Feedback is the last parent of every set,
 * so world misses and other shared cues live there once.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UFeedbackSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Optional. Cues not found here are looked up there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UFeedbackSet> Parent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback", meta = (Categories = "Cue", ForceInlineRow))
	TMap<FGameplayTag, FFeedbackCue> Cues;

	/** This set's own entry for exactly Tag (no parent, no tag fallback). */
	const FFeedbackCue* FindOwnCue(const FGameplayTag& Tag) const { return Cues.Find(Tag); }

	/**
	 * Tag here or in a parent, falling back up the tag (Hit.Pierce -> Hit) within each set
	 * before moving on to the parent. The project's default set is NOT included; the
	 * Feedback Subsystem adds it.
	 */
	const FFeedbackCue* FindCue(const FGameplayTag& Tag) const;

	UFUNCTION(BlueprintPure, Category = "Feedback")
	bool HasCue(FGameplayTag Tag) const { return FindCue(Tag) != nullptr; }

	/** Shared lookup used by the subsystem: one set's chain, most specific tag first. */
	static const FFeedbackCue* FindInChain(const UFeedbackSet* Set, const FGameplayTag& Tag);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
