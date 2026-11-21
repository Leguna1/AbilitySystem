#pragma once

#include "CoreMinimal.h"
#include "AdventureProp.h"
#include "Waystone.generated.h"

/**
 * A resting place: the checkpoint of this game. Using one for the first time attunes it. After that it
 *   - becomes where the player wakes up after dying,
 *   - fills health and focus, and saves the game,
 *   - lets the player travel to any other attuned waystone.
 *
 * Put one in each village and one before each boss zone. Give each its own Waystone Id.
 * The player appears at Arrival Point (the arrow in the editor); keep it clear of the stone.
 */
UCLASS()
class ABILITYSYSTEM_API AWaystone : public AAdventureProp
{
	GENERATED_BODY()

public:
	AWaystone();

	UFUNCTION(BlueprintPure, Category = "Waystone")
	bool IsAttuned() const;

	/** Where the player stands after traveling here or waking up here. */
	UFUNCTION(BlueprintPure, Category = "Waystone")
	FTransform GetArrivalTransform() const;

	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return true; }
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;

	/** For Blueprint looks: the stone lighting up. Also fires (From Save) for waystones attuned in a loaded game. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Waystone")
	void OnAttuned(bool bFromSave);

	/** World flag set when attuned. Must be unique per waystone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone")
	FName WaystoneId = TEXT("Waystone.One");

	/** Attuned from the start (the first village's stone), so it can be traveled to right away. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waystone")
	bool bStartsAttuned = false;

	/** Where arriving players are put. Move it in the level so it's on open ground, facing away from the stone. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<class UArrowComponent> ArrivalPoint;

private:
	void HandleMenuChoice(int32 OptionIndex);
	void ShowMenu();
	void Rest();

	/** The waystones offered on the menu, in the order shown. Option 0 is Rest; travel options follow. */
	TArray<TWeakObjectPtr<AWaystone>> TravelDestinations;
};
