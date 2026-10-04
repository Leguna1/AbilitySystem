#pragma once

#include "CoreMinimal.h"
#include "RangedAttackAbility.h"
#include "ProjectileBarrageAbility.generated.h"

UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UProjectileBarrageAbility : public URangedAttackAbility
{
	GENERATED_BODY()
	UProjectileBarrageAbility();

public:
	
	virtual void ActivateAbility_Implementation() override;
	
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

	
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Launch", meta = (ClampMin = "0.0"))
	float ForwardLaunchStrength = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Launch", meta = (ClampMin = "0.0"))
	float UpwardLaunchStrength = 1.5f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Flight", meta = (ClampMin = "0.0"))
	float BarrageFlightLifespan = 5.0f;

	/**
 * Max random delay (seconds) added to each arrow's redirect, so the volley
 * rains down over time instead of landing on one frame. 0 = all turn together.
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
	
	/** Chance (0..1) per arrow to land on an enemy instead of a random point, before Stat.SeekChance modifiers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Seeking", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseSeekChance = 0.0f;

	/** Enemies within this distance of the volley's center can be sought. Independent of the landing radius. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Seeking", meta = (ClampMin = "0.0"))
	float SeekRadius = 800.0f;

	/** Object channel the seek search looks for. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Seeking")
	TEnumAsByte<ECollisionChannel> SeekObjectChannel = ECC_Pawn;

	/** Targets with any of these status tags are sought first (e.g. Status.Marked). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Seeking")
	FGameplayTagContainer PreferredTargetStatusTags;

	/** Steering toward the sought enemy after the redirect, so moving targets are still hit. 0 = straight shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Barrage|Seeking", meta = (ClampMin = "0.0"))
	float SeekHomingAcceleration = 4000.0f;

private:
	
	UPROPERTY(Transient)
	float ResolvedImpactRadius = 0.0f;
	
	/** Living, targetable enemies near Center: preferred status first, then closest. */
	void FindSeekTargets(const FVector& Center, TArray<AActor*>& OutTargets) const;

	/** The candidate with the fewest arrows so far; ties go to the earlier (preferred/closer) one. */
	static AActor* PickSeekTarget(const TArray<AActor*>& Candidates, TArray<int32>& AssignedCounts);

	UPROPERTY(Transient)
	float ResolvedSeekChance = 0.0f;
};