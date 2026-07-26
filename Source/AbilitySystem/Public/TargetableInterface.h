#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TargetableInterface.generated.h"

UINTERFACE(BlueprintType)
class ABILITYSYSTEM_API UTargetableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by actors that may be selected by UTargetingComponent.
 */
class ABILITYSYSTEM_API ITargetableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Whether this actor may currently be selected.
	 *
	 * Examples:
	 * - alive;
	 * - hostile;
	 * - not hidden;
	 * - not invulnerable to targeting.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
	bool IsTargetable() const;

	/**
	 * World-space point toward which ranged attacks should aim.
	 *
	 * The default Blueprint implementation may return the actor location,
	 * a mesh socket, or a dedicated scene component location.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Targeting")
	FVector GetTargetAimLocation() const;
};