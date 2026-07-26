#pragma once

#include "CoreMinimal.h"
#include "ArrowData.h"
#include "OffensiveAbilityBase.h"
#include "RangedAttackAbility.generated.h"

class UBowComponent;

UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API URangedAttackAbility : public UOffensiveAbilityBase
{
	GENERATED_BODY()

public:
	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	UBowComponent* GetBowComponent() const { return BowComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool HasPreparedProjectile() const;

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool HasReleasedProjectileThisCycle() const { return bProjectileReleased; }

	UFUNCTION(BlueprintPure, Category = "Ability|Ranged")
	bool IsProjectileNocked() const { return bProjectileNocked; }

protected:
	UFUNCTION(BlueprintCallable, Category = "Ability|Ranged")
	void ResetProjectileCycle();

	UFUNCTION(BlueprintCallable, Category = "Ability|Ranged")
	void DiscardPreparedProjectile();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	bool PrepareProjectile();
	virtual bool PrepareProjectile_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	bool NockProjectile();
	virtual bool NockProjectile_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	bool ReleaseProjectile();
	virtual bool ReleaseProjectile_Implementation();

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged")
	FVector ResolveProjectileDirection() const;
	virtual FVector ResolveProjectileDirection_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Ability|Ranged")
	float ResolveProjectileStrength() const;
	virtual float ResolveProjectileStrength_Implementation() const;

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	void OnProjectilePrepared();
	virtual void OnProjectilePrepared_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	void OnProjectileNocked();
	virtual void OnProjectileNocked_Implementation();

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	void OnProjectileReleased(float Strength);
	virtual void OnProjectileReleased_Implementation(float Strength);

	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Ranged")
	void OnRangedAttackFinished(EAbilityEndReason EndReason, bool bHadPreparedProjectile, bool bReleasedProjectile);
	virtual void OnRangedAttackFinished_Implementation(EAbilityEndReason EndReason, bool bHadPreparedProjectile, bool bReleasedProjectile);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile")
	FArrowStats ProjectileStats;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Hand")
	FName ProjectileHandSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Hand")
	FTransform ProjectileHandOffset = FTransform::Identity;

	/**
	 * Optional bow socket used when the Nock event fires.
	 *
	 * Leave NockProjectileEventTag invalid when an ability should keep the
	 * projectile attached to the hand until release.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Bow")
	FName ProjectileBowSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile|Bow")
	FTransform ProjectileBowOffset = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Projectile", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultProjectileStrength = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events")
	FGameplayTag PrepareProjectileEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events")
	FGameplayTag NockProjectileEventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Ranged|Events")
	FGameplayTag ReleaseProjectileEventTag;

private:
	bool HandleProjectileAnimationEvent(FGameplayTag EventTag);

	UPROPERTY(Transient)
	TObjectPtr<UBowComponent> BowComponent;

	UPROPERTY(Transient)
	bool bProjectilePrepared = false;

	UPROPERTY(Transient)
	bool bProjectileNocked = false;

	UPROPERTY(Transient)
	bool bProjectileReleased = false;
};