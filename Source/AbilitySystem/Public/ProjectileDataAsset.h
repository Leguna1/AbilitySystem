#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectileDataAsset.generated.h"

class AProjectileBase;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

/**
 * Defines one kind of projectile: how it looks, flies, hits and ends.
 * An arrow, a magic bolt or a boss orb is the same Projectile actor with a different asset.
 * How much it hurts is up to the ability that fires it, so one asset serves many abilities.
 *
 * Start:   the projectile is launched.
 * Ongoing: it is flying.
 * End:     it hits something.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UProjectileDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UProjectileDataAsset();

	/* -------------------- Projectile -------------------- */

	/** Actor class to fire. Projectile Base works for most; use a Blueprint child for custom components. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<AProjectileBase> ProjectileClass;

	/** Optional. Leave empty for effect-only projectiles (a bolt that is just its Ongoing Effect). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMesh> Mesh;

	/** Half size of the hit box. Zero = keep the projectile class's own hit box. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.0"))
	FVector HitBoxExtent = FVector::ZeroVector;

	/** Sticks into what it hits (arrows). Off = disappears at its first stop (bolts, orbs). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bStickOnImpact = true;

	/** Rolls the mesh while flying. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bSpinInFlight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "bSpinInFlight", Units = "deg"))
	float SpinDegrees = 1080.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (EditCondition = "bSpinInFlight", ClampMin = "0.0", Units = "s"))
	float SpinDuration = 1.0f;

	/* -------------------- Impact -------------------- */

	/** Push given to physics objects it hits, as a multiple of its velocity. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Impact", meta = (ClampMin = "0.0"))
	float ImpactImpulseMultiplier = 1.0f;

	/* -------------------- Movement -------------------- */

	/** Speed at the lowest launch strength. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float MinimumSpeed = 1000.0f;

	/** Speed at full launch strength. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float MaximumSpeed = 5000.0f;

	/** Gravity at full launch strength. 0 = flies straight (most spells). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float MinimumGravityScale = 1.0f;

	/** Gravity at the lowest launch strength. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.0"))
	float MaximumGravityScale = 2.0f;

	/* -------------------- Ongoing feedback -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Ongoing")
	TObjectPtr<UNiagaraSystem> OngoingEffect;

	/** Expected to be a looping sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Ongoing")
	TObjectPtr<USoundBase> OngoingSound;

	/* -------------------- End feedback -------------------- */

	/** Played when the projectile strikes a target that accepts its payload. Misses use the environment impact from Project Settings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|End")
	TObjectPtr<UNiagaraSystem> EndEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|End")
	TObjectPtr<USoundBase> EndSound;

	/** Random pitch range for EndSound, so clustered impacts sound distinct. Both 1.0 = off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|End", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float EndSoundPitchMin = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|End", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float EndSoundPitchMax = 1.0f;

	/* -------------------- Pool-return feedback -------------------- */

	/** Played only when the projectile expires in flight without hitting anything. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Feedback|Pool Return")
	TObjectPtr<UNiagaraSystem> PoolReturnEffect;

	/* -------------------- Lifetime -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Lifetime", meta = (ClampMin = "0.0", Units = "cm"))
	float MaximumUntargetedTravelDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Lifetime", meta = (ClampMin = "0.0", Units = "s"))
	float TargetedFlightLifespan = 10.0f;

	/** Seconds a stuck projectile stays before returning to the pool. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Lifetime", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bStickOnImpact"))
	float ImpactLifespan = 5.0f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
