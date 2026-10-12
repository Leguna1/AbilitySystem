#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.h"
#include "ArrowShotParams.h"
#include "BowBase.generated.h"

class AProjectileBase;
class UProjectileDataAsset;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBowArrowFiredSignature, AProjectileBase*, Arrow, float, ShotStrength);

/**
 * A bow: string draw visuals, and arrows held in the hand and nocked on the string
 * before release. The arrows themselves come from the world's projectile pool.
 */
UCLASS()
class ABILITYSYSTEM_API ABowBase : public AWeaponBase
{
	GENERATED_BODY()

public:
	ABowBase();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Bow|Animation")
	void BeginDrawVisuals();

	UFUNCTION(BlueprintCallable, Category = "Bow|Animation")
	void EndDrawVisuals();

	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	bool AreDrawVisualsActive() const { return bDrawVisualsActive; }

	UFUNCTION(BlueprintPure, Category = "Bow")
	USkeletalMeshComponent* GetBowMesh() const { return BowMesh; }

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool PrepareArrows(UProjectileDataAsset* ArrowData, int32 ArrowCount);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool AttachPreparedArrowToWielder(int32 ArrowIndex, FName SocketName);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool AttachPreparedArrowToBow(int32 ArrowIndex, FName SocketName);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool ReleasePreparedArrows(const TArray<FVector>& Directions, const FArrowShotParams& ShotParams);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	void DiscardPreparedArrows();

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AProjectileBase* GetPreparedArrow(int32 ArrowIndex) const;

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	int32 GetPreparedArrowCount() const { return PreparedArrows.Num(); }

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	bool HasPreparedArrows() const { return !PreparedArrows.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	int32 GetReleasedArrowCount() const { return ReleasedArrows.Num(); }

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AProjectileBase* GetReleasedArrow(int32 ArrowIndex) const;

	UPROPERTY(BlueprintAssignable, Category = "Bow|Events")
	FOnBowArrowFiredSignature OnArrowFired;

	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	float GetDrawAlpha() const { return DrawAlpha; }

	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	FVector GetStringTargetLocation() const { return StringTargetLocation; }

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void OnHolstered_Implementation() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bow|Components")
	TObjectPtr<USkeletalMeshComponent> BowMesh;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|Runtime")
	float DrawAlpha = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Bow|Runtime")
	FVector StringTargetLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation", meta = (ClampMin = "0.0"))
	float DrawInterpSpeed = 15.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation", meta = (ClampMin = "0.0"))
	float ReleaseInterpSpeed = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation")
	FName DrawHandSocketName = TEXT("BowString");

private:
	/** Listens for the arrow going back to the pool, so the bow drops its reference. */
	void TrackArrow(AProjectileBase* Arrow);
	void UntrackArrow(AProjectileBase* Arrow);

	UFUNCTION()
	void HandleArrowReadyToRecycle(AProjectileBase* Arrow);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AProjectileBase>> PreparedArrows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AProjectileBase>> ReleasedArrows;

	bool bDrawVisualsActive = false;
};
