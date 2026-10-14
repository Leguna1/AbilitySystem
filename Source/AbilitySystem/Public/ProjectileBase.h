#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactGroupTypes.h"
#include "ArrowShotParams.h"
#include "ProjectileBase.generated.h"

class AProjectileBase;
class UProjectileDataAsset;
class UAudioComponent;
class UBoxComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProjectileReadyToRecycleSignature, AProjectileBase*, Projectile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnProjectileHitSignature, AProjectileBase*, Projectile, AActor*, HitActor, float, Damage, const FHitResult&, HitResult);

/**
 * Anything fired: arrows, magic bolts, orbs, energy waves. Flies, hits, delivers the
 * shot's payload, pierces, can be redirected or home in, and returns to the pool when done.
 * What it looks like and how it behaves comes from its Projectile Data Asset.
 *
 * Get one from UProjectilePoolSubsystem (or a weapon), then Fire it.
 */
UCLASS()
class ABILITYSYSTEM_API AProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AProjectileBase();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Projectile")
	UProjectileDataAsset* GetProjectileData() const { return ProjectileData; }

	UFUNCTION(BlueprintPure, Category = "Projectile")
	float GetFiredStrength() const { return ShotParams.Strength; }

	/** Damage of this projectile's next hit: the shot's damage after pierce falloff. */
	UFUNCTION(BlueprintPure, Category = "Projectile")
	float GetCalculatedDamage() const;

	UFUNCTION(BlueprintPure, Category = "Projectile")
	bool HasImpacted() const { return bHasImpacted; }

	UFUNCTION(BlueprintPure, Category = "Projectile")
	bool WasTargetedShot() const { return ShotParams.bTargetedShot; }

	UFUNCTION(BlueprintPure, Category = "Projectile")
	const FArrowShotParams& GetShotParams() const { return ShotParams; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Projectile")
	void SpinBegin();
	virtual void SpinBegin_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Projectile")
	bool Fire(const FVector& Direction, const FArrowShotParams& InShotParams);
	virtual bool Fire_Implementation(const FVector& Direction, const FArrowShotParams& InShotParams);

	/* -------------------- Pooling -------------------- */

	/** Sets the projectile up from NewProjectileData. Called by the pool; use UProjectilePoolSubsystem::AcquireProjectile. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Pooling")
	bool ActivateFromPool(UProjectileDataAsset* NewProjectileData);

	UFUNCTION(BlueprintCallable, Category = "Projectile|Pooling")
	void ResetForPool();

	/** Done with this projectile: hides it and makes it available again. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Pooling")
	void ReturnToPool();

	/* -------------------- Movement -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Projectile|Movement")
	bool Redirect(const FVector& NewDirection);

	UFUNCTION(BlueprintPure, Category = "Projectile|Movement")
	bool IsInFlight() const;

	UFUNCTION(BlueprintCallable, Category = "Projectile|Movement")
	bool SetRemainingFlightTime(float Duration);

	/**
	 * Turns the projectile toward TargetPoint after Delay seconds, if it is still in
	 * flight. Runs on the projectile, so it survives the ability that fired it.
	 */
	void ScheduleRedirect(const FVector& TargetPoint, float Delay, bool bDisableGravity);

	/**
	 * Like ScheduleRedirect, but aims at TargetActor's aim point when the redirect
	 * happens, so a moving target is tracked until then. Falls back to FallbackPoint
	 * if the target is gone or dead. HomingAcceleration > 0 keeps steering afterwards.
	 */
	void ScheduleRedirectToActor(AActor* TargetActor, const FVector& FallbackPoint, float Delay, bool bDisableGravity, float HomingAcceleration);

	/* -------------------- Events -------------------- */

	UPROPERTY(BlueprintAssignable, Category = "Projectile|Pooling")
	FOnProjectileReadyToRecycleSignature OnReadyToRecycle;

	UPROPERTY(BlueprintAssignable, Category = "Projectile|Combat")
	FOnProjectileHitSignature OnProjectileHit;

	/** Joins a volley's impact group; the group then decides this projectile's impact sound. */
	void JoinImpactGroup(const FImpactGroupHandle& InImpactGroup);

	/** Increases with every Fire, so listeners can tell this flight from later ones. */
	uint32 GetFlightSerial() const { return FlightSerial; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
	TObjectPtr<UBoxComponent> HitBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Where trails and flight sounds attach. Place it at the front of the mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
	TObjectPtr<USceneComponent> TipLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Projectile|Runtime")
	TObjectPtr<UProjectileDataAsset> ProjectileData;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Projectile|Runtime")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Projectile|Runtime")
	FArrowShotParams ShotParams;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Projectile|Feedback")
	TObjectPtr<UAudioComponent> OngoingSoundRef;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Projectile|Feedback")
	TObjectPtr<UNiagaraComponent> OngoingEffectRef;

private:
	UFUNCTION()
	void HandleHitBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void HandleImpact(AActor* HitActor, UPrimitiveComponent* HitComponent, bool bFromSweep, const FHitResult& SweepResult);

	void StartOngoingFeedback();
	void StopOngoingFeedback();
	void HandleFlightExpired();
	void SpawnPoolReturnEffect();
	void ScheduleFlightExpiry(float Delay);
	void ScheduleRecycle(float Delay);

	void LeaveImpactGroup();
	void PlayImpactFeedback(bool bHitTarget, const FVector& ImpactLocation, const UPrimitiveComponent* HitComponent, const FHitResult& Hit) const;

	void HandleScheduledRedirect();

	UFUNCTION()
	void HandleStuckActorDestroyed(AActor* DestroyedActor);

	/** Stops listening to the actor this projectile is stuck in. */
	void ReleaseStuckActor();

	bool bIsInFlight = false;
	bool bHasImpacted = false;
	bool bIsSpinning = false;

	float SpinElapsedTime = 0.0f;
	float SpinDuration = 1.0f;
	float SpinDegrees = 1080.0f;

	FRotator SpinInitialRotation = FRotator::ZeroRotator;
	FRotator DefaultMeshRotation = FRotator::ZeroRotator;

	/** The class's own hit box size, restored when a data asset doesn't set one. */
	FVector DefaultHitBoxExtent = FVector::ZeroVector;

	FTimerHandle RecycleTimerHandle;

	FImpactGroupHandle ImpactGroup;

	FTimerHandle RedirectTimerHandle;
	FVector RedirectTargetPoint = FVector::ZeroVector;
	bool bRedirectDisablesGravity = false;
	TWeakObjectPtr<AActor> RedirectTargetActor;
	float RedirectHomingAcceleration = 0.0f;

	/** Pierces left this flight. */
	int32 PiercesRemaining = 0;

	/** Damage multiplier for the next target; drops by PierceDamageFactor per target passed through. */
	float CurrentPierceDamageFactor = 1.0f;

	/** Actors already hit this flight, so the projectile never hits one twice. */
	TArray<TWeakObjectPtr<AActor>> HitActors;

	/** Actor this projectile is stuck in, if any. */
	TWeakObjectPtr<AActor> StuckToActor;

	uint32 FlightSerial = 0;
};
