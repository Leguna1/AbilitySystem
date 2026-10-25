// FeedbackTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/HitResult.h"
#include "Templates/SubclassOf.h"
#include "FeedbackTypes.generated.h"

class UCameraShakeBase;
class UMaterialInterface;
class UNiagaraSystem;
class UPrimitiveComponent;
class USoundBase;

/** What one moment looks and sounds like: an effect, a sound, an optional camera shake. Any part can be empty. */
USTRUCT(BlueprintType)
struct ABILITYSYSTEM_API FFeedbackEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector EffectScale = FVector::OneVector;

	/** Hits only: point the effect out of the surface (sparks off a wall). Off = along the hit direction. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	bool bAlignToSurface = true;

	/** Loops expect a looping sound. Give sounds an attenuation and a concurrency so crowds stay clean. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float Volume = 1.0f;

	/** Random pitch range, so repeated hits sound distinct. Both 1 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float PitchMin = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float PitchMax = 1.0f;

	/** Played for cameras within Camera Shake Radius (heavy hits, landings). One-shot cues only. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TSubclassOf<UCameraShakeBase> CameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0.0", Units = "cm"))
	float CameraShakeRadius = 1500.0f;

	bool HasSound() const { return Sound != nullptr; }
	bool HasEffect() const { return Effect != nullptr; }
	bool HasCameraShake() const { return CameraShake.Get() != nullptr; }
	bool IsEmpty() const { return !HasSound() && !HasEffect() && !HasCameraShake(); }

	float RollPitch() const
	{
		return FMath::FRandRange(FMath::Min(PitchMin, PitchMax), FMath::Max(PitchMin, PitchMax));
	}
};

/** Plays instead of the cue's default when the hit surface has this Surface Type. */
USTRUCT(BlueprintType)
struct ABILITYSYSTEM_API FFeedbackSurfaceVariant
{
	GENERATED_BODY()

	/** Set up surface types in Project Settings -> Physics, then give physical materials those types. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	TEnumAsByte<EPhysicalSurface> Surface = SurfaceType_Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
	FFeedbackEffect Effect;
};

/** One entry in a Feedback Set. */
USTRUCT(BlueprintType)
struct ABILITYSYSTEM_API FFeedbackCue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue", meta = (ShowOnlyInnerProperties))
	FFeedbackEffect Effect;

	/** Hits and misses: a different effect per surface (stone sparks, flesh blood, wood splinters). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue|Surfaces")
	TArray<FFeedbackSurfaceVariant> SurfaceVariants;

	/** Loops: socket on the weapon or projectile mesh to attach to. None or missing = its origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue|Loop")
	FName AttachSocket;

	/** Loops: leave the effect behind when stopped, so world-space trails finish instead of vanishing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue|Loop")
	bool bDetachWhenStopped = false;

	/** Loops: seconds the sound takes to fade out when stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue|Loop", meta = (ClampMin = "0.0", Units = "s"))
	float FadeOutTime = 0.25f;

	/**
	 * Loops: drawn over the mesh the loop attaches to while it runs (an electric shell on a
	 * lightning arrow, ice on a frozen character, glow on an enchanted blade). Removed when it ends.
	 * Use a translucent or fresnel material made for overlays.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue|Loop")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	/** Loops: played once where the loop attaches, when it starts (a status lands, an aura ignites). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue|Loop")
	FFeedbackEffect StartEffect;

	/** Loops: played once where the loop was, when it ends (a status wears off, ice shatters). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue|Loop")
	FFeedbackEffect StopEffect;

	/** The variant for Surface, or the default effect. */
	const FFeedbackEffect& GetEffectFor(const EPhysicalSurface Surface) const
	{
		if (Surface != SurfaceType_Default)
		{
			for (const FFeedbackSurfaceVariant& Variant : SurfaceVariants)
			{
				if (Variant.Surface == Surface)
				{
					return Variant.Effect;
				}
			}
		}

		return Effect;
	}
};

/** Where and against what a cue plays. */
USTRUCT(BlueprintType)
struct ABILITYSYSTEM_API FFeedbackCueContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FVector Location = FVector::ZeroVector;

	/** Direction of the action (shot direction, swing direction). Used unless the effect aligns to the surface. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FRotator Rotation = FRotator::ZeroRotator;

	/** Hits: surface normal and physical material. Empty for launches, swings and draws. */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	FHitResult Hit;

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	TWeakObjectPtr<AActor> Instigator;

	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	TWeakObjectPtr<AActor> Target;

	/**
	 * Float user parameters handed to the spawned Niagara system (a field's Radius, a hit's
	 * Strength). Add matching User Parameters to the system; missing ones are ignored.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Feedback")
	TMap<FName, float> NiagaraFloats;

	/** What was struck, when it isn't in Hit (overlaps). Used to find the surface type. */
	TWeakObjectPtr<const UPrimitiveComponent> HitComponent;
};
