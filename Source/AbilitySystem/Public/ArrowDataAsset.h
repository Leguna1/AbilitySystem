#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ArrowDataAsset.generated.h"

class AArrowBase;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;

/**
 * Defines an arrow's gameplay values and presentation.
 *
 * Start:
 * The arrow is launched.
 *
 * Ongoing:
 * The arrow is flying.
 *
 * End:
 * The arrow impacts something.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UArrowDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/* -------------------- Projectile -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Projectile")
	TSubclassOf<AArrowBase> ArrowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Projectile")
	TObjectPtr<UStaticMesh> ArrowMesh;

	/* -------------------- Damage -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Damage", meta = (ClampMin = "0.0"))
	float MaximumDamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Damage", meta = (ClampMin = "0.0"))
	float ImpactImpulseMultiplier = 1.0f;

	/* -------------------- Movement -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Movement", meta = (ClampMin = "0.0"))
	float MinimumSpeed = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Movement", meta = (ClampMin = "0.0"))
	float MaximumSpeed = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Movement", meta = (ClampMin = "0.0"))
	float MinimumGravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Movement", meta = (ClampMin = "0.0"))
	float MaximumGravityScale = 2.0f;

	/* -------------------- Start feedback -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|Start")
	TObjectPtr<UNiagaraSystem> StartEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|Start")
	TObjectPtr<USoundBase> StartSound;

	/* -------------------- Ongoing feedback -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|Ongoing")
	TObjectPtr<UNiagaraSystem> OngoingEffect;

	/**
	 * Expected to be a looping sound when used.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|Ongoing")
	TObjectPtr<USoundBase> OngoingSound;

	/* -------------------- End feedback -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|End")
	TObjectPtr<UNiagaraSystem> EndEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|End")
	TObjectPtr<USoundBase> EndSound;

	/**
	 * Min/max random pitch multiplier applied to EndSound per impact. Keeping the
	 * two values apart (e.g. 0.92 / 1.08) makes clustered volley impacts sound
	 * distinct rather than one stuttering source. Set both to 1.0 to disable.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|End", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float EndSoundPitchMin = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|End", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float EndSoundPitchMax = 1.0f;

	/* -------------------- Pool-return feedback -------------------- */

	/**
	 * Played only when the arrow expires in flight without impacting.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Feedback|Pool Return")
	TObjectPtr<UNiagaraSystem> PoolReturnEffect;

	/* -------------------- Lifetime -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Lifetime", meta = (ClampMin = "0.0"))
	float MaximumUntargetedTravelDistance = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Lifetime", meta = (ClampMin = "0.0"))
	float TargetedFlightLifespan = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Lifetime", meta = (ClampMin = "0.0"))
	float ImpactLifespan = 5.0f;
};