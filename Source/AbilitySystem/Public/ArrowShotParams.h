#pragma once

#include "CoreMinimal.h"
#include "ImpactGroupTypes.h"
#include "PayloadReceiver.h"
#include "ArrowShotParams.generated.h"

class UFeedbackSet;

/** Per-shot values the firing ability hands every arrow it releases. The arrow keeps them for its whole flight. */
USTRUCT(BlueprintType)
struct FArrowShotParams
{
	GENERATED_BODY()

	/** 0..1 draw strength. Drives the projectile's speed and gravity (its damage is already in Damage). */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	float Strength = 1.0f;

	/** Aimed at the current target: expires by the targeted lifespan instead of travel distance. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	bool bTargetedShot = false;

	/**
	 * Damage of each projectile of this shot, decided by the firing ability (rank, modifiers, charge).
	 * A multiple of the instigator's power when it has one, else flat. Pierce falloff applies on top.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	float Damage = 1.0f;

	/** Impact group this shot's impacts report to. Invalid = ungrouped. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	FImpactGroupHandle ImpactGroup;
	
	/** Living targets this arrow passes through before stopping. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	int32 PierceCount = 0;

	/** Damage factor per target passed through: 0.5 = each next target takes half of the previous. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	float PierceDamageFactor = 0.5f;
	
	/** What each hit of this arrow carries besides damage. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	FHitSpec Hit;

	/**
	 * This shot's look, over the projectile data's own set: cues it defines win, the rest come
	 * from the projectile's set. Null = the projectile's set only.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	TObjectPtr<UFeedbackSet> FeedbackSet;

	/** 0..1 how hard this shot is, sent to hit effects as the "Strength" Niagara parameter (charge level). */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	float FeedbackStrength = 1.0f;
};