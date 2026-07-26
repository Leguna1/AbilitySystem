#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTargetChangedSignature, AActor*, PreviousTarget, AActor*, NewTarget);

/**
 * Basic automatic RPG targeting component.
 *
 * The component periodically searches for actors implementing
 * UTargetableInterface, filters them by range and forward angle, then selects
 * the best candidate.
 *
 * Targeting is independent from ability execution. Ranged abilities query the
 * current target only when resolving projectile direction.
 */
UCLASS(ClassGroup = (Targeting), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingComponent();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void RefreshTarget();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ClearTarget();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool SetTarget(AActor* NewTarget);

	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool HasTarget() const { return IsValid(CurrentTarget); }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsValidTarget(AActor* Candidate) const;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FVector GetCurrentTargetAimLocation() const;

	UFUNCTION(BlueprintPure, Category = "Targeting")
	FVector GetDirectionToCurrentTarget(const FVector& Origin) const;

	UPROPERTY(BlueprintAssignable, Category = "Targeting|Events")
	FTargetChangedSignature OnTargetChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Chooses the best target from the supplied candidates.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Targeting")
	AActor* SelectBestTarget(const TArray<AActor*>& Candidates) const;
	virtual AActor* SelectBestTarget_Implementation(const TArray<AActor*>& Candidates) const;

	/**
	 * Additional project-specific filtering.
	 *
	 * The base implementation returns true.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Targeting")
	bool PassesCustomTargetFilter(AActor* Candidate) const;
	virtual bool PassesCustomTargetFilter_Implementation(AActor* Candidate) const;

	/**
	 * Returns the target's world-space aim point.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Targeting")
	FVector ResolveTargetAimLocation(AActor* Target) const;
	virtual FVector ResolveTargetAimLocation_Implementation(AActor* Target) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search", meta = (ClampMin = "0.0"))
	float AcquisitionRange = 2500.0f;

	/**
	 * Existing targets remain valid until they exceed this distance.
	 *
	 * Keeping this slightly larger than AcquisitionRange prevents rapid target
	 * loss and reacquisition near the boundary.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search", meta = (ClampMin = "0.0"))
	float ReleaseRange = 2800.0f;

	/**
	 * Full horizontal targeting cone in degrees.
	 *
	 * 120 means 60 degrees to either side of the forward direction.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float AcquisitionAngle = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search", meta = (ClampMin = "0.01"))
	float RefreshInterval = 0.15f;

	/**
	 * Object types included by the overlap search.
	 *
	 * Add Pawn for character targets.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search")
	TArray<TEnumAsByte<EObjectTypeQuery>> TargetObjectTypes;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search")
	TSubclassOf<AActor> TargetClassFilter;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

	/**
	 * Relative influence of screen-facing alignment versus distance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Scoring", meta = (ClampMin = "0.0"))
	float AngleScoreWeight = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Scoring", meta = (ClampMin = "0.0"))
	float DistanceScoreWeight = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	bool bAutoRefresh = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Debug")
	bool bDrawTargetDebugSphere = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Debug", meta = (ClampMin = "0.0"))
	float TargetDebugSphereRadius = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Debug", meta = (ClampMin = "4"))
	int32 TargetDebugSphereSegments = 12;

private:
	void FindTargetCandidates(TArray<AActor*>& OutCandidates) const;
	bool IsInsideAcquisitionAngle(AActor* Candidate) const;
	bool HasLineOfSightTo(AActor* Candidate) const;
	float CalculateTargetScore(AActor* Candidate) const;
	FVector GetTargetingOrigin() const;
	FVector GetTargetingForwardVector() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting|Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> CurrentTarget;

	FTimerHandle RefreshTimerHandle;
};