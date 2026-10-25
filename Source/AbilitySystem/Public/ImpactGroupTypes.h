#pragma once

#include "CoreMinimal.h"

#include "ImpactGroupTypes.generated.h"

UENUM(BlueprintType)
enum class EImpactGroupAudio : uint8
{
	/** The shared sound plays once for the first hit and once for the first miss. */
	OncePerGroup UMETA(DisplayName = "Once Per Group"),

	/** The shared sound plays at most once per Window seconds, per result. */
	OncePerWindow UMETA(DisplayName = "Once Per Window"),

	/** The shared sound plays on every impact. */
	PerImpact UMETA(DisplayName = "Per Impact")
};

UENUM(BlueprintType)
enum class EImpactResult : uint8
{
	/** The target accepted the payload. */
	Hit UMETA(DisplayName = "Hit"),

	/** World geometry, or a target that refused the payload. */
	Miss UMETA(DisplayName = "Miss")
};

UENUM(BlueprintType)
enum class EImpactHitFeedback : uint8
{
	/** Each hit plays its own sound and effect. */
	SoundAndEffect UMETA(DisplayName = "Sound And Effect"),

	/** Each hit shows its own effect; sound is left to the group. */
	EffectOnly UMETA(DisplayName = "Effect Only"),

	/** Hits play nothing themselves; only the group's shared sound plays. */
	None UMETA(DisplayName = "None")
};

/** Lightweight reference to an open impact group. */
USTRUCT(BlueprintType)
struct FImpactGroupHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = INDEX_NONE;

	bool IsValid() const { return Id != INDEX_NONE; }
};

/**
 * How one ability use's hits play together: a volley, a cleave, a burst.
 * What they look and sound like comes from the hitter's Feedback Set; this only
 * decides how much of it plays when many land at once.
 *
 * The shared sound is Cue.Group.Hit / Cue.Group.Miss from the hitter's set. When no set
 * defines one, the hit's own sound plays in its place, as often as Policy allows.
 */
USTRUCT(BlueprintType)
struct FImpactGroupSettings
{
	GENERATED_BODY()

	/** How much of each hit's own feedback still plays inside the group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Feedback")
	EImpactHitFeedback EachHit = EImpactHitFeedback::EffectOnly;

	/** How often the shared sound plays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Feedback")
	EImpactGroupAudio Policy = EImpactGroupAudio::OncePerWindow;

	/** Hits landing within this many seconds share one sound. Covers a volley landing and a rapid burst alike. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Feedback", meta = (ClampMin = "0.0", Units = "s", EditCondition = "Policy == EImpactGroupAudio::OncePerWindow"))
	float Window = 0.15f;
};
