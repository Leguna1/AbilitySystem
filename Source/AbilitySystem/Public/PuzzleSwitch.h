#pragma once

#include "CoreMinimal.h"
#include "AdventureProp.h"
#include "PayloadReceiver.h"
#include "PuzzleSwitch.generated.h"

class AProgressGate;
class UBoxComponent;

/** How a Puzzle Switch is worked. */
UENUM(BlueprintType)
enum class ESwitchActivation : uint8
{
	/** A lever: walk up and press Interact. */
	Interact UMETA(DisplayName = "Interact (lever)"),

	/** A crystal: hit it with anything that deals damage. Arrows reach switches the player can't walk to. */
	Hit UMETA(DisplayName = "Hit (crystal)"),

	/** A pressure plate: stand on it. */
	Step UMETA(DisplayName = "Step (pressure plate)")
};

/**
 * Levers, crystals and pressure plates. Turning one on opens its Targets (Progress Gates) and can set a
 * world flag; turning it off closes them again.
 *
 * Puzzles come from the options:
 *   - Reset After: the switch turns itself off after some seconds, so the player must run for the door.
 *   - Can Turn Off: using it again turns it off (a lever that swaps which of two doors is open:
 *     give one of the doors Starts Open... or use two switches).
 *   - Step + Hold To Stay On: the plate is only on while something stands on it (the companion can hold it).
 *   - Hit: place it across a gap so only an arrow reaches it.
 */
UCLASS()
class ABILITYSYSTEM_API APuzzleSwitch : public AAdventureProp, public IPayloadReceiver
{
	GENERATED_BODY()

public:
	APuzzleSwitch();

	UFUNCTION(BlueprintPure, Category = "Switch")
	bool IsOn() const { return bOn; }

	UFUNCTION(BlueprintCallable, Category = "Switch")
	void SetOn(bool bNewOn);

	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

	/** Hit switches take the hit (and toggle); the others let it pass as a miss. */
	virtual bool ReceivePayload_Implementation(const FAbilityPayload& Payload) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** For Blueprint looks: a lever moving, a crystal lighting up, a click. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Switch")
	void OnSwitchChanged(bool bNowOn);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	ESwitchActivation Activation = ESwitchActivation::Interact;

	/** Gates opened while the switch is on. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Switch")
	TArray<TObjectPtr<AProgressGate>> Targets;

	/** World flag set the first time the switch turns on (for gates elsewhere, or a quest objective). Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	FName FlagToSet;

	/** Using an on switch turns it off again. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	bool bCanTurnOff = false;

	/** Seconds until it turns itself off. 0 = stays on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch", meta = (ClampMin = "0.0", Units = "s"))
	float ResetAfter = 0.0f;

	/** Pressure plates only: on while a pawn stands on it, off when the last one steps away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch", meta = (EditCondition = "Activation == ESwitchActivation::Step"))
	bool bHoldToStayOn = false;

	/** Pressure plates only: what counts as standing on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> StepArea;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	FLinearColor OffColor = FLinearColor(0.9f, 0.35f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Switch")
	FLinearColor OnColor = FLinearColor(0.2f, 0.9f, 1.0f);

private:
	UFUNCTION()
	void HandleStepBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStepEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** What using it does: on, or off again if it can be turned off. */
	void Toggle();
	void TurnOffByTimer();

	bool bOn = false;
	FTimerHandle ResetTimer;
};
