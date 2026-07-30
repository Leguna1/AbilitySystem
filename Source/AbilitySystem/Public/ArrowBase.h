#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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
	
	UFUNCTION(BlueprintCallable, Category = "Arrow|Feedback")
	void PlayStartFeedback();

	UFUNCTION(BlueprintPure, Category = "Arrow")
	float GetFiredStrength() const { return FiredStrength; }

	UFUNCTION(BlueprintPure, Category = "Arrow")
	float GetCalculatedDamage() const;

	UFUNCTION(BlueprintPure, Category = "Arrow")
	bool HasImpacted() const { return bHasImpacted; }

	UFUNCTION(BlueprintPure, Category = "Arrow")
	bool WasTargetedShot() const { return bWasTargetedShot; }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Arrow")
	void SpinBegin();
	virtual void SpinBegin_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Arrow")
	bool Fire(const FVector& Direction, float Strength, bool bTargetedShot);
	virtual bool Fire_Implementation(const FVector& Direction, float Strength, bool bTargetedShot);

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
	float FiredStrength = 0.0f;

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
	void PlayEndFeedback(const FVector& FeedbackLocation);
	void HandleFlightExpired();
	void SpawnPoolReturnEffect();
	void ScheduleFlightExpiry(float Delay);
	void ScheduleRecycle(float Delay);

	bool bIsInFlight = false;
	bool bHasImpacted = false;
	bool bWasTargetedShot = false;
	bool bIsSpinning = false;

	float SpinElapsedTime = 0.0f;
	float SpinDuration = 1.0f;
	float SpinDegrees = 1080.0f;

	FRotator SpinInitialRotation = FRotator::ZeroRotator;
	FRotator DefaultArrowMeshRotation = FRotator::ZeroRotator;

	FTimerHandle RecycleTimerHandle;
};