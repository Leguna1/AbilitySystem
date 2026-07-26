#pragma once

#include "CoreMinimal.h"
#include "ArrowData.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * Per-shot arrow configuration supplied by the executing ability.
 *
 * The bow applies these values when preparing an arrow from its pool.
 * Different abilities can therefore use different damage, movement, and
 * effect values while sharing the same physical bow and arrow class.
 */
USTRUCT(BlueprintType)
struct FArrowStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow")
	FName ArrowName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Damage", meta = (ClampMin = "0.0"))
	float MaximumDamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Movement", meta = (ClampMin = "0.0"))
	float SpeedMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Movement", meta = (ClampMin = "0.0"))
	float GravityMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Impact", meta = (ClampMin = "0.0"))
	float ImpactImpulseMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Effects")
	TObjectPtr<USoundBase> WhooshSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Effects")
	TObjectPtr<UNiagaraSystem> TrailEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Effects")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Effects")
	TObjectPtr<UNiagaraSystem> ImpactEffect;
};