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
	
	/**
	 * Flat-damage hits (the attacker has no power for the damage type) are scaled by a random factor
	 * in [1 - this, 1 + this]. Hits that scale with power roll between its Min and Max instead. 0 = no variance.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float DamageVariance = 0.07f;

	/**
	 * Damage reduction = Defense / (Defense + this), for Armor and Magic Resist alike.
	 * Defense equal to this halves the damage. At 100, each point is +1% effective health against that damage type.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Defense", meta = (ClampMin = "1.0"))
	float DefenseConstant = 100.0f;

	/** The most Armor or Magic Resist can reduce damage. 0.75 = at most 75% less damage. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat|Defense", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float MaxDamageReduction = 0.75f;
	
	/** Damage kept per target an arrow passes through (0.5 = half), before Stat.PierceDamage modifiers. A Pierce feature can override it. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DefaultPierceDamageFactor = 0.5f;
};