#pragma once

#include "CoreMinimal.h"
#include "ImpactGroupTypes.h"
#include "ModifierTypes.h"
#include "HitReactionTypes.h"
#include "ArrowShotParams.generated.h"

/** Per-shot values the firing ability hands every arrow it releases. The arrow keeps them for its whole flight. */
USTRUCT(BlueprintType)
struct FArrowShotParams
{
	GENERATED_BODY()

	/** 0..1 draw strength. Drives speed, gravity and the arrow data's strength damage multiplier. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	float Strength = 1.0f;

	/** Aimed at the current target: expires by the targeted lifespan instead of travel distance. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	bool bTargetedShot = false;

	/** The ability's share of the damage, applied on top of the arrow data's. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	float DamageMultiplier = 1.0f;

	/** Impact group this shot's impacts report to. Invalid = ungrouped. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	FImpactGroupHandle ImpactGroup;
	
	/** Tags of the firing ability, passed on to every payload this arrow delivers. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	FGameplayTagContainer SourceAbilityTags;

	/** Statuses this arrow applies on hit. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	TArray<FStatusApplication> Statuses;
	
	/** Living targets this arrow passes through before stopping. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	int32 PierceCount = 0;

	/** Damage factor per target passed through: 0.5 = each next target takes half of the previous. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	float PierceDamageFactor = 0.5f;
	
	/** Impact every hit of this arrow carries. */
	UPROPERTY(BlueprintReadWrite, Category = "Arrow|Shot")
	FHitImpact Impact;
};