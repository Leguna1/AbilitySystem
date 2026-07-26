#pragma once

#include "CoreMinimal.h"
#include "ArrowData.h"
#include "GameFramework/Actor.h"
#include "ArrowBase.generated.h"

class UBoxComponent;
class UCameraComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class UProjectileMovementComponent;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;

class AArrowBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnArrowReadyToRecycleSignature, AArrowBase*, Arrow);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnArrowHitSignature, AArrowBase*, Arrow, AActor*, HitActor, float, Damage, const FHitResult&, HitResult);

USTRUCT(BlueprintType)
struct FArrowConfiguration
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float MinSpeed = 1000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0"))
	float MaxSpeed = 5000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float MinGravity = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float MaxGravity = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<USoundBase> WhooshSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> TrailEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<USoundBase> ArrowImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifetime", meta = (ClampMin = "0.0"))
	float FlyingLifespan = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifetime", meta = (ClampMin = "0.0"))
	float ImpactLifespan = 5.0f;
};

/**
 * Physical arrow projectile.
 *
 * The arrow owns:
 * - projectile movement;
 * - impact detection;
 * - visual and audio effects;
 * - impact attachment;
 * - recycling state.
 *
 * The arrow reports hits but does not apply gameplay damage itself.
 */
UCLASS()
class ABILITYSYSTEM_API AArrowBase : public AActor
{
	GENERATED_BODY()

public:
	AArrowBase();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Arrow")
	void SetArrowStats(const FArrowStats& NewArrowStats);

	UFUNCTION(BlueprintPure, Category = "Arrow")
	const FArrowStats& GetArrowStats() const { return ArrowStats; }

	UFUNCTION(BlueprintPure, Category = "Arrow")
	float GetFiredStrength() const { return FiredStrength; }

	UFUNCTION(BlueprintPure, Category = "Arrow")
	float GetCalculatedDamage() const;

	UFUNCTION(BlueprintPure, Category = "Arrow")
	bool IsInFlight() const { return bIsInFlight; }

	UFUNCTION(BlueprintPure, Category = "Arrow")
	bool HasImpacted() const { return bHasImpacted; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Arrow")
	void SpinBegin();
	virtual void SpinBegin_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Arrow")
	bool Fire(const FVector& Direction, float Strength);
	virtual bool Fire_Implementation(const FVector& Direction, float Strength);

	/**
	 * Reactivates an available pooled arrow and applies per-shot stats.
	 *
	 * Collision remains disabled until Fire is called.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arrow|Pooling")
	void ActivateFromPool(const FArrowStats& NewArrowStats);

	/**
	 * Fully disables and hides the arrow while preserving the actor instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arrow|Pooling")
	void ResetForPool();

	/**
	 * Resets the arrow and reports it as available to its owning pool.
	 */
	UFUNCTION(BlueprintCallable, Category = "Arrow|Pooling")
	void ReturnToPool();

	UPROPERTY(BlueprintAssignable, Category = "Arrow|Pooling")
	FOnArrowReadyToRecycleSignature OnReadyToRecycle;

	UPROPERTY(BlueprintAssignable, Category = "Arrow|Combat")
	FOnArrowHitSignature OnArrowHit;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Arrow|Configuration")
	FArrowConfiguration ArrowConfiguration;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arrow|Runtime")
	FArrowStats ArrowStats;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arrow|Runtime")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Arrow|Runtime")
	float FiredStrength = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Arrow|Runtime")
	TObjectPtr<UNiagaraComponent> TrailEffectRef;

private:
	UFUNCTION()
	void HandleHitBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void HandleImpact(AActor* HitActor, UPrimitiveComponent* HitComponent, bool bFromSweep, const FHitResult& SweepResult);
	void DestroyTrailEffect();
	void ScheduleRecycle(float Delay);

	bool bIsInFlight = false;
	bool bHasImpacted = false;
	bool bIsSpinning = false;

	float SpinElapsedTime = 0.0f;
	float SpinDuration = 1.0f;
	float SpinDegrees = 1080.0f;

	FRotator SpinInitialRotation = FRotator::ZeroRotator;
	FRotator DefaultArrowMeshRotation = FRotator::ZeroRotator;

	FTimerHandle TrailDestroyTimerHandle;
	FTimerHandle RecycleTimerHandle;
};