#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "MontageAbility.h"
#include "DirectionalDodgeAbility.generated.h"

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
	virtual bool CanReplaceActiveAbility_Implementation(const UAbility* CurrentAbility) const override;

	UFUNCTION(BlueprintPure, Category = "Ability|Dodge")
	FVector GetDodgeDirection() const { return DodgeDirection; }

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
};