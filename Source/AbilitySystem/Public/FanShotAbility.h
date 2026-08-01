#pragma once

#include "CoreMinimal.h"
#include "RangedAttackAbility.h"
#include "FanShotAbility.generated.h"

/** How the horizontal fan spread is defined. */
UENUM(BlueprintType)
enum class EFanSpreadMode : uint8
{
	/** TotalSpreadAngle is divided evenly across all arrows. Total stays fixed as count changes. */
	TotalAngle UMETA(DisplayName = "Total Angle"),

	/** AngleBetweenArrows is fixed. Total spread grows as arrow count grows. */
	AnglePerArrow UMETA(DisplayName = "Angle Per Arrow")
};

/**
 * Releases every prepared arrow simultaneously in a horizontal fan centered on
 * the player's aim (control rotation), spread around the world up axis.
 *
 * Arrow count is socket-driven: it equals the number of configured hand/bow
 * sockets on RangedAttackAbility. Skill-tree "more arrows" upgrades add sockets.
 *
 * Unlike Barrage, there is no arc, redirect, or ground targeting: this is a
 * single flat volley fired outward on release.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UFanShotAbility : public URangedAttackAbility
{
	GENERATED_BODY()

protected:
	/** A fan aims by spread rather than at a single target, so never home to the current target. */
	virtual bool ShouldUseCurrentTarget_Implementation() const override;

	/** Computes this arrow's outgoing direction as an offset from the aim center. */
	virtual FVector ResolveProjectileDirectionForIndex_Implementation(int32 ProjectileIndex) const override;

	/** Single-burst ability: commit (spend cost, start cooldown) when the fan is released. */
	virtual void OnProjectileReleased_Implementation(float Strength) override;

	/** Center direction the fan spreads around. Defaults to control-rotation forward, then actor forward. */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Fan Shot")
	FVector ResolveFanCenterDirection() const;
	virtual FVector ResolveFanCenterDirection_Implementation() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Fan Shot")
	EFanSpreadMode SpreadMode = EFanSpreadMode::TotalAngle;

	/** Total fan width in degrees, used when SpreadMode is TotalAngle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Fan Shot", meta = (ClampMin = "0.0", ClampMax = "180.0", EditCondition = "SpreadMode == EFanSpreadMode::TotalAngle", EditConditionHides))
	float TotalSpreadAngle = 60.0f;

	/** Fixed gap in degrees between adjacent arrows, used when SpreadMode is AnglePerArrow. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Fan Shot", meta = (ClampMin = "0.0", ClampMax = "90.0", EditCondition = "SpreadMode == EFanSpreadMode::AnglePerArrow", EditConditionHides))
	float AngleBetweenArrows = 12.0f;

private:
	/** Signed yaw offset (degrees) for an arrow index given the total arrow count. */
	float ComputeYawOffsetForIndex(int32 ProjectileIndex, int32 ProjectileCount) const;
};