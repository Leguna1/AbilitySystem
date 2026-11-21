#pragma once

#include "CoreMinimal.h"
#include "AdventureProp.h"
#include "ProgressGate.generated.h"

class UItemDataAsset;

/**
 * A barrier that opens when the world allows it. Three ways, usable together:
 *   - Required Flags: opens by itself once all are set (an altar's Altar Id seals a boss zone; a quest's
 *     Flag On Complete opens a village gate; a boss's Boss Id opens the way onward).
 *   - Key Item: the player unlocks it with a key from the bag (a dungeon's locked door).
 *   - A Puzzle Switch that lists it under Targets opens (and can close) it.
 *
 * Scale the actor to fill the passage. Open = no collision and hidden. Give it a Gate Id if it should
 * stay open after loading a save (key doors; flag gates reopen from their flags anyway).
 *
 * Enemies walk through an opened gate only if the project's navmesh is dynamic
 * (Project Settings -> Navigation Mesh -> Runtime Generation: Dynamic).
 */
UCLASS()
class ABILITYSYSTEM_API AProgressGate : public AAdventureProp
{
	GENERATED_BODY()

public:
	AProgressGate();

	UFUNCTION(BlueprintPure, Category = "Gate")
	bool IsOpen() const { return bOpen; }

	/** Opens the gate. Announce = a message for the player (off for gates restored from a save). */
	UFUNCTION(BlueprintCallable, Category = "Gate")
	void OpenGate(bool bAnnounce = true);

	/** Closes it again (a timed switch running out). */
	UFUNCTION(BlueprintCallable, Category = "Gate")
	void CloseGate();

	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** For Blueprint looks: a door swinging, a barrier dissolving, a sound. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Gate")
	void OnGateStateChanged(bool bNowOpen);

	/** Optional. Set when opened, and a gate whose id is already set starts open. Needed for key doors to stay open across saves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FName GateId;

	/** World flags that open the gate once all are set: an Altar Id, a Boss Id, a quest's Flag On Complete. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	TArray<FName> RequiredFlags;

	/** Shown while closed and not openable by hand, e.g. "Sealed by the Altar of Dawn". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FText SealedHint;

	/** Item that unlocks the gate by hand. Empty = it can't be unlocked by hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate|Key")
	TObjectPtr<UItemDataAsset> KeyItem;

	/** The key is used up (Zelda's small keys). Off = it stays in the bag (a master key). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate|Key")
	bool bConsumeKey = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	bool bStartsOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gate")
	FLinearColor SealedColor = FLinearColor(0.9f, 0.15f, 0.15f);

private:
	UFUNCTION()
	void HandleFlagSet(FName Flag);

	bool HasKey(const AActor* Interactor) const;
	void ApplyState();

	bool bOpen = false;
};
