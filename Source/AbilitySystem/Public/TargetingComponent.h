#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

class ACharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTargetChangedSignature, AActor*, PreviousTarget, AActor*, NewTarget);

/**
 * Runtime record of how much targeting "intent" the player has expressed
 * toward a given actor. Accumulated from camera framing (primary) and
 * movement engagement (secondary), then decayed over time.
 *
 * Not exposed to Blueprint; the weak pointer keeps it GC-safe without a
 * UPROPERTY, and the array is transient runtime state.
 */
struct FTargetIntentRecord
{
	TWeakObjectPtr<AActor> Target;
	float IntentScore = 0.0f;
};

/**
 * Automatic RPG targeting component for an orient-to-movement archer.
 *
 * Instead of scoring purely by distance/angle, the component reads player
 * intent from two axes:
 *   - Primary: where the free-look camera is framed. Candidates near the
 *     look direction accrue intent, gated by how settled the camera is
 *     (a sweeping/scanning camera contributes little).
 *   - Secondary: how the character moves relative to each candidate. Closing
 *     on or strafing around an enemy reads as engagement; this is chiefly
 *     what disambiguates kiting, since backpedalling while the camera holds
 *     on an enemy is still "engaging that one".
 *
 * Intent accumulates per candidate and decays, so the held target is sticky
 * but swaps naturally once the player commits look/movement to a new enemy.
 *
 * Acquisition is 360 degrees by default: with orient-to-movement the character
 * faces whatever gets picked, so gating candidates by current facing would be
 * circular and stop you ever targeting a flank. All directionality lives in
 * the intent pass, not in the acquisition cone.
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
	 * Chooses the highest-intent candidate from the supplied set.
	 *
	 * Swap hysteresis (the stickiness bonus for the held target) is applied by
	 * the caller in RefreshTarget, so this stays a clean "best of set" that a
	 * Blueprint can override without having to reason about the current lock.
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
	 * Full horizontal acquisition cone in degrees.
	 *
	 * Defaults to 360: with orient-to-movement the character faces whatever
	 * gets picked, so a forward-facing pre-filter would prevent targeting a
	 * flank. Directionality is handled by the intent pass, not this cone.
	 * Tighten it only if you deliberately want to restrict acquisition.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Search", meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float AcquisitionAngle = 360.0f;

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
	 * Full cone in degrees around the CAMERA look direction within which a
	 * candidate accrues framing intent. Keep it tighter than AcquisitionAngle
	 * so not everything on screen accumulates; 70 means 35 degrees either side.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Intent", meta = (ClampMin = "1.0", ClampMax = "360.0"))
	float IntentConeAngle = 70.0f;

	/**
	 * Camera angular speed (deg/s) at which a look counts as fully "sweeping"
	 * rather than a committed frame. Framing intent scales to zero as the
	 * camera's turn rate approaches this.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Intent", meta = (ClampMin = "1.0"))
	float MaxCameraSweepSpeed = 180.0f;

	/**
	 * Minimum planar speed (cm/s) below which movement contributes no
	 * engagement intent (the archer is effectively standing still).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Intent", meta = (ClampMin = "0.0"))
	float MinIntentSpeed = 150.0f;

	/**
	 * Movement heading change rate (deg/s) at which movement counts as a
	 * dodge/jink rather than committed engagement. Engagement intent scales to
	 * zero as the heading turn rate approaches this. This is what separates a
	 * deliberate approach/strafe from a panic dodge.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Intent", meta = (ClampMin = "1.0"))
	float MaxJinkAngularSpeed = 270.0f;

	/**
	 * Weight of the movement/engagement term relative to camera framing.
	 * Keep below 1 so framing stays the dominant signal.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Intent", meta = (ClampMin = "0.0"))
	float EngagementWeight = 0.4f;

	/**
	 * Fraction of accumulated intent remaining after one second. Lower is a
	 * snappier release; higher is a stickier lock. This knob dominates feel.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Intent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntentDecayRate = 0.2f;

	/**
	 * Upper clamp on accumulated intent, so a long-framed target can still be
	 * overtaken within a bounded time instead of becoming a permanent lock.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Intent", meta = (ClampMin = "0.01"))
	float MaxIntentScore = 3.0f;

	/**
	 * Bonus added to the held target when comparing it against a challenger,
	 * damping flip-flopping between two near-equal candidates.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Intent", meta = (ClampMin = "0.0"))
	float TargetStickinessBonus = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting")
	bool bAutoRefresh = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Debug")
	bool bDrawTargetDebugSphere = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Debug", meta = (ClampMin = "0.0"))
	float TargetDebugSphereRadius = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Targeting|Debug", meta = (ClampMin = "4"))
	int32 TargetDebugSphereSegments = 12;

private:
	void FindTargetCandidates(TArray<AActor*>& OutCandidates) const;
	bool IsInsideAcquisitionAngle(AActor* Candidate) const;
	bool HasLineOfSightTo(AActor* Candidate) const;
	FVector GetTargetingOrigin() const;
	FVector GetTargetingForwardVector() const;

	/** Accumulate and decay per-candidate intent for this refresh. */
	void UpdateIntent(const TArray<AActor*>& Candidates, float DeltaTime);

	/** Current accumulated intent for an actor, or 0 if none is recorded. */
	float GetAccumulatedIntent(const AActor* Target) const;

	/** Mutable lookup used while accumulating; nullptr if not yet recorded. */
	FTargetIntentRecord* FindIntentRecord(const AActor* Target);

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Targeting|Runtime", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<AActor> CurrentTarget;

	TArray<FTargetIntentRecord> IntentRecords;

	FVector PreviousCameraForward = FVector::ForwardVector;
	FVector PreviousMoveDir = FVector::ForwardVector;

	FTimerHandle RefreshTimerHandle;
};