#pragma once

#include "CoreMinimal.h"
#include "BowBase.h"
#include "BowDataAsset.h"
#include "Components/ActorComponent.h"
#include "BowComponent.generated.h"

class AArrowBase;
class ABowBase;
class ACharacter;
class UArrowDataAsset;
class UBowDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBowComponentArrowFiredSignature, AArrowBase*, Arrow, float, ShotStrength);

UCLASS(ClassGroup = (BowArrow), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UBowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBowComponent();

	UFUNCTION(BlueprintCallable, Category = "Bow")
	bool EquipBow();

	UFUNCTION(BlueprintCallable, Category = "Bow")
	void UnequipBow();

	UFUNCTION(BlueprintPure, Category = "Bow")
	ABowBase* GetBow() const { return Bow; }

	UFUNCTION(BlueprintPure, Category = "Bow")
	bool HasEquippedBow() const { return IsValid(Bow); }

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool PrepareArrow(UArrowDataAsset* ArrowData);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool AttachPreparedArrowToWielder(FName SocketName, const FTransform& RelativeOffset);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool AttachPreparedArrowToBow(FName SocketName, const FTransform& RelativeOffset);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	bool ReleasePreparedArrow(const FVector& Direction, float Strength, bool bTargetedShot);

	UFUNCTION(BlueprintCallable, Category = "Bow|Arrow")
	void DiscardPreparedArrow();

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AArrowBase* GetPreparedArrow() const;

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AArrowBase* GetLastFiredArrow() const;

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	bool HasPreparedArrow() const;

	UPROPERTY(BlueprintAssignable, Category = "Bow|Events")
	FOnBowComponentArrowFiredSignature OnArrowFired;
	
	UFUNCTION(BlueprintCallable, Category = "Bow|Animation")
	void BeginDrawVisuals();

	UFUNCTION(BlueprintCallable, Category = "Bow|Animation")
	void EndDrawVisuals();

	UFUNCTION(BlueprintCallable, Category = "Bow|Feedback")
	void HandleFeedbackPoint(EBowFeedbackPoint FeedbackPoint, UBowDataAsset* BowData);

	UFUNCTION(BlueprintCallable, Category = "Bow|Feedback")
	void ClearAllFeedback();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Configuration")
	TSubclassOf<ABowBase> BowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Configuration")
	FName BowSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Configuration")
	bool bEquipBowOnBeginPlay = true;

private:
	UFUNCTION()
	void HandleBowArrowFired(AArrowBase* Arrow, float ShotStrength);

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<ABowBase> Bow;
};