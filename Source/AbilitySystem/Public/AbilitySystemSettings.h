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
};