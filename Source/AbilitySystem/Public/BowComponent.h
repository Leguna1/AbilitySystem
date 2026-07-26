#pragma once

#include "CoreMinimal.h"
#include "ArrowData.h"
#include "BowBase.h"
#include "Components/ActorComponent.h"
#include "BowComponent.generated.h"

class AArrowBase;
class ABowBase;
class ACharacter;

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

	UFUNCTION(BlueprintCallable, Category = "Bow|Visuals")
	void BeginDrawVisuals();

	UFUNCTION(BlueprintCallable, Category = "Bow|Visuals")
	void EndDrawVisuals();

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
	AArrowBase* GetPreparedArrow() const;

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	AArrowBase* GetLastFiredArrow() const;

	UFUNCTION(BlueprintPure, Category = "Bow|Arrow")
	bool HasPreparedArrow() const;

	UPROPERTY(BlueprintAssignable, Category = "Bow|Events")
	FOnBowComponentArrowFiredSignature OnArrowFired;

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