// SwordBase.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SwordBase.generated.h"

class UPrimitiveComponent;
class UBoxComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSwordHitSignature, AActor*, HitActor, const FHitResult&, Hit);

/**
 * A melee weapon actor. Owns its mesh and a hit-detection collision box that is
 * OFF by default and toggled on only during a swing's active frames (driven by
 * the melee ability). While active, overlaps report unique hit actors once per
 * swing and are broadcast via OnSwordHit for the ability to turn into payloads.
 *
 * The sword does not know about damage or abilities -- it only reports "I
 * overlapped this actor during an active swing." The ability decides what that
 * means, keeping the framework decoupled.
 */
UCLASS()
class ABILITYSYSTEM_API ASwordBase : public AActor
{
	GENERATED_BODY()

public:
	ASwordBase();

	/** Begin a swing: clears the already-hit set and enables overlap detection. */
	UFUNCTION(BlueprintCallable, Category = "Sword")
	void BeginHitDetection();

	/** End a swing: disables overlap detection. */
	UFUNCTION(BlueprintCallable, Category = "Sword")
	void EndHitDetection();

	UFUNCTION(BlueprintPure, Category = "Sword")
	bool IsHitDetectionActive() const { return bHitDetectionActive; }

	/** Broadcast once per unique actor hit during an active swing. */
	UPROPERTY(BlueprintAssignable, Category = "Sword|Events")
	FOnSwordHitSignature OnSwordHit;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHitBoxOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UStaticMeshComponent> SwordMesh;

	/** Overlap volume along the blade. Disabled unless a swing is active. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UBoxComponent> HitBox;

private:
	bool bHitDetectionActive = false;

	/** Actors already hit this swing, so each takes one hit per swing. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> HitActorsThisSwing;
};