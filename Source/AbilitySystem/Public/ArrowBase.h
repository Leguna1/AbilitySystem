#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ImpactGroupTypes.h"
#include "ArrowShotParams.h"
#include "ArrowBase.generated.h"

class AArrowBase;
class UArrowDataAsset;
class UAudioComponent;
class UBoxComponent;
class UCameraComponent;
class UNiagaraComponent;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArrowReadyToRecycleSignature, AArrowBase*, Arrow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnArrowHitSignature, AArrowBase*, Arrow, AActor*, HitActor, float, Damage, const FHitResult&, HitResult);

UCLASS()
class ABILITYSYSTEM_API AArrowBase : public AActor
{
	GENERATED_BODY()

public:
	AArrowBase();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Arrow")
	UArrowDataAsset* GetArrowData() const { return ArrowData; }
	

	UFUNCTION(BlueprintPure, Category = "Arrow")
	float GetFiredStrength() const { return ShotParams.Strength; }

	UFUNCTION(BlueprintPure, Category = "Arrow")
	float GetCalculatedDamage() const;

	UFUNCTION(BlueprintPure, Category = "Arrow")
	bool HasImpacted() const { return bHasImpacted; }

	UFUNCTION(BlueprintPure, Category = "Arrow")
	bool WasTargetedShot() const { return ShotParams.bTargetedShot; }
	
	UFUNCTION(BlueprintPure, Category = "Arrow")
	const FArrowShotParams& GetShotParams() const { return ShotParams; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Arrow")
	void SpinBegin();
	virtual void SpinBegin_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Arrow")
	bool Fire(const FVector& Direction, const FArrowShotParams& InShotParams);
	virtual bool Fire_Implementation(const FVector& Direction, const FArrowShotParams& InShotParams);

	UFUNCTION(BlueprintCallable, Category = "Arrow|Pooling")
	bool ActivateFromPool(UArrowDataAsset* NewArrowData);

	UFUNCTION(BlueprintCallable, Category = "Arrow|Pooling")
	void ResetForPool();

	UFUNCTION(BlueprintCallable, Category = "Arrow|Pooling")
	void ReturnToPool();
	
	UFUNCTION(BlueprintCallable, Category = "Arrow|Movement")
	bool Redirect(const FVector& NewDirection);

	UFUNCTION(BlueprintPure, Category = "Arrow|Movement")
	bool IsInFlight() const;
	
	UFUNCTION(BlueprintCallable, Category = "Arrow|Movement")
	bool SetRemainingFlightTime(float Duration);

	UPROPERTY(BlueprintAssignable, Category = "Arrow|Pooling")
	FOnArrowReadyToRecycleSignature OnReadyToRecycle;

	UPROPERTY(BlueprintAssignable, Category = "Arrow|Combat")
	FOnArrowHitSignature OnArrowHit;
	
	/** Joins a volley's impact group; the group then decides this arrow's impact sound. */
	void JoinImpactGroup(const FImpactGroupHandle& InImpactGroup);
	
	/**
 * Turns the arrow toward TargetPoint after Delay seconds, if it is still in
 * flight. Runs on the arrow, so it survives the ability that fired it.
 */
	void ScheduleRedirect(const FVector& TargetPoint, float Delay, bool bDisableGravity);
	
	/**
 * Like ScheduleRedirect, but aims at TargetActor's aim point when the redirect
 * happens, so a moving target is tracked until then. Falls back to FallbackPoint
 * if the target is gone or dead. HomingAcceleration > 0 keeps steering afterwards.
 */
	void ScheduleRedirectToActor(AActor* TargetActor, const FVector& FallbackPoint, float Delay, bool bDisableGravity, float HomingAcceleration);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow|Components")
	TObjectPtr<UBoxComponent> HitBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow|Components")
	TObjectPtr<UCameraComponent> KillCam;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow|Components")
	TObjectPtr<UStaticMeshComponent> ArrowMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow|Components")
	TObjectPtr<USceneComponent> TipLocation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arrow|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arrow|Runtime")
	TObjectPtr<UArrowDataAsset> ArrowData;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arrow|Runtime")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arrow|Runtime")
	FArrowShotParams ShotParams;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Arrow|Feedback")
	TObjectPtr<UAudioComponent> OngoingSoundRef;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Arrow|Feedback")
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

	bool bIsInFlight = false;
	bool bHasImpacted = false;
	bool bIsSpinning = false;

	float SpinElapsedTime = 0.0f;
	float SpinDuration = 1.0f;
	float SpinDegrees = 1080.0f;

	FRotator SpinInitialRotation = FRotator::ZeroRotator;
	FRotator DefaultArrowMeshRotation = FRotator::ZeroRotator;

	FTimerHandle RecycleTimerHandle;
	
	

	void LeaveImpactGroup();

	FImpactGroupHandle ImpactGroup;
	
	void PlayImpactFeedback(bool bHitTarget, const FVector& ImpactLocation, const UPrimitiveComponent* HitComponent, const FHitResult& Hit) const;
	
	void HandleScheduledRedirect();

	FTimerHandle RedirectTimerHandle;
	FVector RedirectTargetPoint = FVector::ZeroVector;
	bool bRedirectDisablesGravity = false;
	
	/** Pierces left this flight. */
	int32 PiercesRemaining = 0;

	/** Damage multiplier for the next target; drops by PierceDamageFactor per target passed through. */
	float CurrentPierceDamageFactor = 1.0f;

	/** Actors already hit this flight, so the arrow never hits one twice. */
	TArray<TWeakObjectPtr<AActor>> HitActors;
	
	UFUNCTION()
	void HandleStuckActorDestroyed(AActor* DestroyedActor);

	/** Stops listening to the actor this arrow is stuck in. */
	void ReleaseStuckActor();

	/** Actor this arrow is stuck in, if any. */
	TWeakObjectPtr<AActor> StuckToActor;
	
	TWeakObjectPtr<AActor> RedirectTargetActor;
	float RedirectHomingAcceleration = 0.0f;
};