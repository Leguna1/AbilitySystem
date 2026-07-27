#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "BowAnimInstance.generated.h"

class ABowBase;

UCLASS()
class ABILITYSYSTEM_API UBowAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	/**
	 * World-space location toward which the center string bone is pulled.
	 *
	 * This value is calculated and owned by ABowBase. The AnimGraph only
	 * consumes it.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Bow|String")
	FVector StringTargetLocation = FVector::ZeroVector;

	/**
	 * Blending amount used by the AnimGraph.
	 *
	 * 0 = string at rest.
	 * 1 = string fully following StringTargetLocation.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Bow|String")
	float DrawAlpha = 0.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<ABowBase> OwningBow;
};