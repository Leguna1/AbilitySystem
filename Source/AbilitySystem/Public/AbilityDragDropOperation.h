#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "AbilityDragDropOperation.generated.h"

class UAbility;

/** An ability being dragged: from a skill tree node (Source Slot -1) or from an action bar slot. */
UCLASS()
class ABILITYSYSTEM_API UAbilityDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	TSubclassOf<UAbility> AbilityClass;

	/** The action bar slot it came from, or -1 when dragged from outside the bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Action Bar")
	int32 SourceSlotIndex = INDEX_NONE;
};
