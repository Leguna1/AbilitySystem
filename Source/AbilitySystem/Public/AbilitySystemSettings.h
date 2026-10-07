#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AbilitySystemSettings.generated.h"

class UNiagaraSystem;
class USoundBase;

/** Project Settings -> Game -> Ability System. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Ability System"))
class ABILITYSYSTEM_API UAbilitySystemSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Played for every miss: world geometry, or a target that refused the payload. Shared by all weapons. */
	UPROPERTY(Config, EditAnywhere, Category = "Impacts|Environment")
	TSoftObjectPtr<USoundBase> EnvironmentImpactSound;

	UPROPERTY(Config, EditAnywhere, Category = "Impacts|Environment")
	TSoftObjectPtr<UNiagaraSystem> EnvironmentImpactEffect;

	UPROPERTY(Config, EditAnywhere, Category = "Impacts|Environment", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float EnvironmentPitchMin = 0.95f;

	UPROPERTY(Config, EditAnywhere, Category = "Impacts|Environment", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float EnvironmentPitchMax = 1.05f;
	
	/** Every delivered hit is scaled by a random factor in [1 - this, 1 + this]. 0 = no variance. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DamageVariance = 0.07f;
	
	/** Damage kept per target an arrow passes through (0.5 = half), before Stat.PierceDamage modifiers. A Pierce feature can override it. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DefaultPierceDamageFactor = 0.5f;
};