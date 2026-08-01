#pragma once

#include "CoreMinimal.h"
#include "RangedAttackAbility.h"
#include "ProjectileBarrageAbility.generated.h"

UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UProjectileBarrageAbility : public URangedAttackAbility
{
	GENERATED_BODY()

public:
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;

protected:
	virtual FVector ResolveProjectileDirectionForIndex_Implementation(int32 ProjectileIndex) const override;
	virtual bool ShouldUseCurrentTarget_Implementation() const override;
	virtual void OnProjectileReleased_Implementation(float Strength) override;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Barrage")
	FVector ResolveBarrageTargetCenter() const;
	virtual FVector ResolveBarrageTargetCenter_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Barrage")
	FVector ResolveBarrageImpactPoint(int32 ProjectileIndex, int32 ProjectileCount, const FVector& TargetCenter) const;
	virtual FVector ResolveBarrageImpactPoint_Implementation(int32 ProjectileIndex, int32 ProjectileCount, const FVector& TargetCenter) const;

	UFUNCTION(BlueprintCallable, Category = "Ability|Barrage")
	void RedirectReleasedProjectiles();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Launch", meta = (ClampMin = "0.0"))
	float ForwardLaunchStrength = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Launch", meta = (ClampMin = "0.0"))
	float UpwardLaunchStrength = 1.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Flight", meta = (ClampMin = "0.0"))
	float BarrageFlightLifespan = 5.0f;

	/**
	 * Max random flight-time added per arrow (seconds), spreading landings over
	 * time so their impact sounds don't stack and phase-cancel. Each arrow gets a
	 * random offset in [0, this]. ~0.3-0.5 turns a mushy simultaneous hit into a
	 * distinct patter. 0 = all land together (original behaviour).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Flight", meta = (ClampMin = "0.0"))
	float BarrageImpactStagger = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Launch", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float LaunchSpreadAngle = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Launch", meta = (ClampMin = "0.0"))
	float RedirectDelay = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Targeting", meta = (ClampMin = "0.0"))
	float DefaultTargetDistance = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Targeting", meta = (ClampMin = "0.0"))
	float ImpactRadius = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Targeting", meta = (ClampMin = "0.0"))
	float GroundTraceHeight = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Targeting", meta = (ClampMin = "0.0"))
	float GroundTraceDepth = 3000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Targeting")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

private:
	void ClearBarrageTimer();

	UPROPERTY(Transient)
	TArray<FVector> BarrageImpactPoints;

	FTimerHandle RedirectTimerHandle;
};