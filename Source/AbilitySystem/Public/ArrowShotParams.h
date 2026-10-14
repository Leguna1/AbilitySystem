#pragma once

#include "CoreMinimal.h"
#include "ImpactGroupTypes.h"
#include "PayloadReceiver.h"
#include "ArrowShotParams.generated.h"

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
};