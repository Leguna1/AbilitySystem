#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AbilityTargetSource.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UAbilityTargetSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Tells a character's abilities who it is attacking. Implemented by AI controllers;
 * the player uses its Targeting Component instead.
 */
class ABILITYSYSTEM_API IAbilityTargetSource
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ability|Targeting")
	AActor* GetAbilityTarget() const;
};