#pragma once

#include "CoreMinimal.h"
#include "ProjectileDataAsset.h"
#include "ArrowDataAsset.generated.h"

/**
 * Kept so existing arrow assets keep loading. Everything lives in Projectile Data Asset now;
 * new projectiles (arrows included) can be made from Projectile Data Asset directly.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API UArrowDataAsset : public UProjectileDataAsset
{
	GENERATED_BODY()
};
