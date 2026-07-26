#pragma once

#include "CoreMinimal.h"
#include "ArrowBase.h"
#include "ArrowData.h"
#include "GameFramework/Actor.h"
#include "BowBase.generated.h"

class AArrowBase;
class UAudioComponent;
class USkeletalMeshComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBowArrowFiredSignature, AArrowBase*, Arrow, float, ShotStrength);

UCLASS()
class ABILITYSYSTEM_API ABowBase : public AActor
{
	GENERATED_BODY()

public:
	ABowBase();

	/* -------------------- Bow visuals -------------------- */

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Bow|Visuals")
	void BeginDrawVisuals();
	virtual void BeginDrawVisuals_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Bow|Visuals")
	void EndDrawVisuals();
	virtual void EndDrawVisuals_Implementation();

	UFUNCTION(BlueprintPure, Category = "Bow|Visuals")
	bool AreDrawVisualsActive() const { return bDrawVisualsActive; }

	UFUNCTION(BlueprintPure, Category = "Bow")
	USkeletalMeshComponent* GetBowMesh() const { return BowMesh; }

	/* -------------------- Wielder setup -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Bow|Setup")
	void SetWielderMesh(USkeletalMeshComponent* InWielderMesh);

	UFUNCTION(BlueprintPure, Category = "Bow|Setup")
	USkeletalMeshComponent* GetWielderMesh() const { return WielderMesh; }

	/* -------------------- Prepared arrow -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool PrepareArrow(const FArrowStats& ArrowStats);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool AttachPreparedArrowToWielder(FName SocketName, const FTransform& RelativeOffset);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool AttachPreparedArrowToBow(FName SocketName, const FTransform& RelativeOffset);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool ReleasePreparedArrow(const FVector& Direction, float Strength);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	void DiscardPreparedArrow();

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AArrowBase* GetPreparedArrow() const { return PreparedArrow; }

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AArrowBase* GetLastFiredArrow() const { return LastFiredArrow; }

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	bool HasPreparedArrow() const { return IsValid(PreparedArrow); }

	UPROPERTY(BlueprintAssignable, Category = "Bow|Events")
	FOnBowArrowFiredSignature OnArrowFired;

	/* -------------------- Pool -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Bow|Pool")
	void InitializeArrowPool();

	UFUNCTION(BlueprintPure, Category = "Bow|Pool")
	int32 GetSpawnedArrowCount() const { return AllSpawnedArrows.Num(); }

	UFUNCTION(BlueprintPure, Category = "Bow|Pool")
	int32 GetAvailableArrowCount() const { return AvailableArrows.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bow|Components")
	TObjectPtr<USkeletalMeshComponent> BowMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Visuals")
	TObjectPtr<USoundBase> DrawSound;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow|Visuals")
	TObjectPtr<UAudioComponent> DrawSoundRef;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Arrow")
	TSubclassOf<AArrowBase> ArrowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Pool", meta = (ClampMin = "1"))
	int32 InitialArrowPoolSize = 6;

private:
	AArrowBase* CreateArrow();
	AArrowBase* AcquireAvailableArrow();
	void DestroyArrowPool();

	UFUNCTION()
	void HandleArrowReadyToRecycle(AArrowBase* Arrow);

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> WielderMesh;

	UPROPERTY(Transient)
	TObjectPtr<AArrowBase> PreparedArrow;

	UPROPERTY(Transient)
	TObjectPtr<AArrowBase> LastFiredArrow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AArrowBase>> AllSpawnedArrows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AArrowBase>> AvailableArrows;

	bool bDrawVisualsActive = false;
};