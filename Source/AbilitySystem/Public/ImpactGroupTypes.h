#pragma once

#include "CoreMinimal.h"

#include "ImpactGroupTypes.generated.h"

class USoundBase;
class UNiagaraSystem;


UENUM(BlueprintType)
enum class EImpactGroupAudio : uint8
{
	/** Group feedback plays once for the first hit and once for the first miss. */
	OncePerGroup UMETA(DisplayName = "Once Per Group"),

	/** Group feedback plays at most once per Window seconds, per result. */
	OncePerWindow UMETA(DisplayName = "Once Per Window"),

	/** Group feedback plays on every impact. */
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
enum class EProjectileImpactFeedback : uint8
{
	/** Each impact plays its own sound and effect. */
	SoundAndEffect UMETA(DisplayName = "Sound And Effect"),

	/** Each impact shows its own effect; sound is left to the group. */
	EffectOnly UMETA(DisplayName = "Effect Only"),

	/** Impacts play nothing themselves; only the group's feedback plays. */
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
/** A sound + effect pair for one impact. */
USTRUCT(BlueprintType)
struct FImpactFeedback
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float PitchMin = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float PitchMax = 1.0f;
};

/** How one action's impacts (a volley, a swing) look and sound together. */
USTRUCT(BlueprintType)
struct FImpactGroupSettings
{
	GENERATED_BODY()

	/** How much of each impact's own feedback still plays inside the group. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Group")
	EProjectileImpactFeedback ProjectileFeedback = EProjectileImpactFeedback::EffectOnly;

	/**
	 * Played for the group's hits, as often as Policy allows. With no sound set,
	 * the impact's own hit sound plays instead (unless impacts already play their own).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Group")
	FImpactFeedback GroupHit;

	/** Same as GroupHit, for misses. With no sound set, the environment sound plays instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Group")
	FImpactFeedback GroupMiss;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Group")
	EImpactGroupAudio Policy = EImpactGroupAudio::OncePerGroup;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact Group", meta = (ClampMin = "0.0", EditCondition = "Policy == EImpactGroupAudio::OncePerWindow"))
	float Window = 0.25f;
};

