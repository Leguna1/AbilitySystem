#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ProjectilePoolSubsystem.generated.h"

class AProjectileBase;
class UProjectileDataAsset;

/**
 * Keeps every projectile in the world for reuse, shared by all shooters.
 * Projectiles outlive whoever fired them (a dead witch's bolt keeps flying,
 * arrows stay stuck after a weapon swap), so the world owns them, not a weapon.
 *
 * Nothing to set up: ask for a projectile with AcquireProjectile, Fire it, and it
 * comes back here by itself when it's done.
 */
UCLASS()
class ABILITYSYSTEM_API UProjectilePoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UProjectilePoolSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * A ready projectile set up from Data, owned by NewOwner (it never hits its owner or
	 * instigator). Fire it, or ReturnToPool if it ends up unused. Null on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Pool")
	AProjectileBase* AcquireProjectile(UProjectileDataAsset* Data, AActor* NewOwner, APawn* NewInstigator);

	/** Spawns hidden projectiles for Data up front, so the first shots don't spawn mid-fight. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Pool")
	void PrewarmProjectiles(UProjectileDataAsset* Data, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Projectile|Pool")
	int32 GetSpawnedCount() const { return AllProjectiles.Num(); }

	UFUNCTION(BlueprintPure, Category = "Projectile|Pool")
	int32 GetAvailableCount() const { return AvailableProjectiles.Num(); }

	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	AProjectileBase* SpawnPooledProjectile(TSubclassOf<AProjectileBase> ProjectileClass);
	AProjectileBase* TakeAvailable(TSubclassOf<AProjectileBase> ProjectileClass);

	UFUNCTION()
	void HandleProjectileReadyToRecycle(AProjectileBase* Projectile);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AProjectileBase>> AllProjectiles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AProjectileBase>> AvailableProjectiles;
};
