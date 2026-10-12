#pragma once

#include "CoreMinimal.h"
#include "ProjectileBase.h"
#include "ArrowBase.generated.h"

class UCameraComponent;

/**
 * An arrow: a projectile with a kill cam. Flight, hits, sticking and pooling all
 * come from Projectile Base; how it looks and behaves comes from its data asset.
 */
UCLASS()
class ABILITYSYSTEM_API AArrowBase : public AProjectileBase
{
	GENERATED_BODY()

public:
	AArrowBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow|Components")
	TObjectPtr<UCameraComponent> KillCam;
};
