#pragma once

#include "CoreMinimal.h"
#include "ArrowBase.h"
#include "BowDataAsset.h"
#include "GameFramework/Actor.h"
#include "BowBase.generated.h"

class AArrowBase;
class UArrowDataAsset;
class UAudioComponent;
class UNiagaraComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBowArrowFiredSignature, AArrowBase*, Arrow, float, ShotStrength);

UENUM()
enum class EBowFeedbackSetType : uint8
{
	Start,
	Ongoing,
	End
};
USTRUCT()
struct FBowFeedbackRuntime
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Sound;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Effect;
};
UCLASS()
class ABILITYSYSTEM_API ABowBase : public AActor
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

	UFUNCTION(BlueprintCallable, Category = "Bow|Feedback")
	void HandleFeedbackPoint(EBowFeedbackPoint FeedbackPoint, UBowDataAsset* InBowData);

	UFUNCTION(BlueprintCallable, Category = "Bow|Feedback")
	void ClearAllFeedback();

	UFUNCTION(BlueprintPure, Category = "Bow")
	USkeletalMeshComponent* GetBowMesh() const { return BowMesh; }

	UFUNCTION(BlueprintCallable, Category = "Bow|Setup")
	void SetWielderMesh(USkeletalMeshComponent* InWielderMesh);

	UFUNCTION(BlueprintPure, Category = "Bow|Setup")
	USkeletalMeshComponent* GetWielderMesh() const { return WielderMesh; }

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool PrepareArrows(UArrowDataAsset* ArrowData, int32 ArrowCount);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool AttachPreparedArrowToWielder(int32 ArrowIndex, FName SocketName);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool AttachPreparedArrowToBow(int32 ArrowIndex, FName SocketName);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool ReleasePreparedArrows(const TArray<FVector>& Directions, float Strength, bool bTargetedShot);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	void DiscardPreparedArrows();

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AArrowBase* GetPreparedArrow(int32 ArrowIndex) const;

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	int32 GetPreparedArrowCount() const { return PreparedArrows.Num(); }

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	bool HasPreparedArrows() const { return !PreparedArrows.IsEmpty(); }
	
	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	int32 GetReleasedArrowCount() const { return ReleasedArrows.Num(); }

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AArrowBase* GetReleasedArrow(const int32 ArrowIndex) const;

	UPROPERTY(BlueprintAssignable, Category = "Bow|Events")
	FOnBowArrowFiredSignature OnArrowFired;

	UFUNCTION(BlueprintPure, Category = "Bow|Pool")
	int32 GetSpawnedArrowCount() const { return AllSpawnedArrows.Num(); }

	UFUNCTION(BlueprintPure, Category = "Bow|Pool")
	int32 GetAvailableArrowCount() const { return AvailableArrows.Num(); }

	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	float GetDrawAlpha() const { return DrawAlpha; }

	UFUNCTION(BlueprintPure, Category = "Bow|Animation")
	FVector GetStringTargetLocation() const { return StringTargetLocation; }
	
	

protected:
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Bow|Components")
	TObjectPtr<USkeletalMeshComponent> BowMesh;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Bow|Feedback")
	TObjectPtr<UBowDataAsset> ActiveBowData;

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
	AArrowBase* CreateArrow(TSubclassOf<AArrowBase> ArrowClass);
	AArrowBase* AcquireAvailableArrow(TSubclassOf<AArrowBase> ArrowClass);

	void ProcessFeedbackSet(EBowFeedbackSetType SetType, const FBowFeedbackSet& FeedbackSet, EBowFeedbackPoint FeedbackPoint);
	void ActivateFeedbackSet(EBowFeedbackSetType SetType, const FBowFeedbackSet& FeedbackSet);
	void ClearFeedbackSet(EBowFeedbackSetType SetType);
	void DestroyArrowPool();

	FBowFeedbackRuntime& GetFeedbackRuntime(EBowFeedbackSetType SetType);

	UFUNCTION()
	void HandleArrowReadyToRecycle(AArrowBase* Arrow);

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> WielderMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AArrowBase>> PreparedArrows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AArrowBase>> ReleasedArrows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AArrowBase>> AllSpawnedArrows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AArrowBase>> AvailableArrows;

	UPROPERTY(Transient)
	FBowFeedbackRuntime StartFeedbackRuntime;

	UPROPERTY(Transient)
	FBowFeedbackRuntime OngoingFeedbackRuntime;

	UPROPERTY(Transient)
	FBowFeedbackRuntime EndFeedbackRuntime;

	bool bDrawVisualsActive = false;
};