#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "MontageAbility.h"
#include "DirectionalDodgeAbility.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;

/** One dodge feedback set: an effect + sound, optionally attached to the character. */
USTRUCT(BlueprintType)
struct FDodgeFeedbackSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UNiagaraSystem> Effect = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<USoundBase> Sound = nullptr;

	/**
	 * When true, effect and sound attach to the character and move with the roll
	 * (use for the ongoing trail). When false, they spawn at the dodge start
	 * location in world (use for start burst / landing burst).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	bool bAttachToCharacter = false;
};

/**
 * Plays one forward-roll montage toward the player's current movement-input
 * direction.
 *
 * Before activation, the ability sweeps the character capsule through the
 * intended dodge path. Any blocking hit rejects the dodge.
 *
 * The montage should normally use root motion.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UDirectionalDodgeAbility : public UMontageAbility
{
	GENERATED_BODY()

public:
	UDirectionalDodgeAbility();

	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;
	virtual bool CanReplaceActiveAbility_Implementation(const UAbility* CurrentAbility) const override;

	UFUNCTION(BlueprintPure, Category = "Ability|Dodge")
	FVector GetDodgeDirection() const { return DodgeDirection; }

	/** Dodge warps along its computed dodge direction, not actor-forward. */
	virtual FVector GetRootMotionWarpDirection_Implementation() const override;

protected:
	/**
	 * Converts the two-dimensional movement input into a normalized
	 * world-space direction using the controller's yaw.
	 */
	bool CalculateDodgeDirection(FVector& OutDirection, bool& bOutDodgeBackward) const;

	/**
	 * Returns true only when the character capsule can sweep through the
	 * complete configured dodge distance without a blocking hit.
	 */
	bool IsDodgePathClear(const FVector& Direction, FHitResult* OutHit = nullptr) const;

	/**
	 * Called after direction and path validation, immediately before the
	 * montage starts.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Ability|Dodge")
	void OnDodgePrepared(FVector Direction);
	virtual void OnDodgePrepared_Implementation(FVector Direction);

	/**
	 * Approximate horizontal distance travelled by the roll montage.
	 *
	 * Set this to match the montage's root-motion displacement.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge", meta = (ClampMin = "0.0"))
	float DodgeDistance = 450.0f;

	/**
	 * Additional clearance kept between the swept capsule and an obstacle.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge", meta = (ClampMin = "0.0"))
	float ObstaclePadding = 10.0f;

	/**
	 * Collision channel used to test the dodge path.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge")
	TEnumAsByte<ECollisionChannel> DodgeTraceChannel = ECC_Visibility;

	/**
	 * Rejects dodging while airborne when false.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge")
	bool bAllowAirDodge = false;

	/**
	 * Stops existing movement velocity immediately before the roll starts.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge")
	bool bStopMovementBeforeDodge = true;

	/**
	 * Rotates the actor instantly toward the dodge direction before playing
	 * the forward-roll montage.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge")
	bool bRotateTowardDodgeDirection = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge")
	FName ForwardDodgeSection = TEXT("Forward");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge")
	FName BackwardDodgeSection = TEXT("Backward");
	
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Ability|Dodge", meta = (AllowPrivateAccess = "true"))
	bool bDodgeBackward = false;

private:
	UPROPERTY(Transient)
	FVector DodgeDirection = FVector::ZeroVector;

	/* -------------------- Feedback -------------------- */

protected:
	/** Fired at dodge start (burst). Spawns Start set; world or attached per its flag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge|Feedback")
	FDodgeFeedbackSet StartFeedback;

	/** Ongoing trail during the roll (usually attached). Stopped when the dodge ends. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge|Feedback")
	FDodgeFeedbackSet OngoingFeedback;

	/** Fired when the dodge ends (landing burst). Spawns End set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Dodge|Feedback")
	FDodgeFeedbackSet EndFeedback;

	/** Extra start feedback in Blueprint (on top of the Start set). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Dodge|Feedback")
	void OnDodgeStartFeedback(FVector Direction, FVector StartLocation);

	/** Extra ongoing feedback in Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Dodge|Feedback")
	void OnDodgeOngoingFeedback(FVector Direction);

	/** Extra end feedback in Blueprint. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Dodge|Feedback")
	void OnDodgeEndFeedback(FVector EndLocation);

private:
	/** Spawns a feedback set; attached to the character or at WorldLocation per its flag. */
	void PlayDodgeFeedbackSet(const FDodgeFeedbackSet& Set, const FVector& WorldLocation, bool bTrackOngoing);

	/** Stops and clears the tracked ongoing (attached) feedback, if any. */
	void StopOngoingDodgeFeedback();

	/** Tracked ongoing components so they can be stopped when the dodge ends. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> OngoingEffectComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> OngoingSoundComponent;

	UPROPERTY(Transient)
	FVector DodgeStartLocation = FVector::ZeroVector;
};