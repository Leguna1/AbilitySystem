#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

UINTERFACE(BlueprintType)
class ABILITYSYSTEM_API UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by anything the player can walk up to and use with the Interact key:
 * chests, altars, waystones, levers, locked doors, villagers.
 *
 * The actor also has to register itself with the World Progress Subsystem (Register Interactable)
 * so the player finds it; Adventure Prop and NPC Character do that for you.
 */
class ABILITYSYSTEM_API IInteractableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Text shown while the player stands next to this, e.g. "Open chest" or "Sealed by the Altar of Dawn".
	 * Empty = nothing to show (an opened chest), and the actor is skipped.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FText GetInteractionPrompt(AActor* Interactor) const;

	/** Whether pressing Interact does something now. False still shows the prompt, as a hint (a locked door). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* Interactor) const;

	/** The player pressed Interact. Interactor is the player's pawn. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);
};
