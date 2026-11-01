#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "InventoryDragDropOperation.generated.h"

class UInventoryComponent;
class UItemDataAsset;

/** Where a dragged item came from. */
UENUM(BlueprintType)
enum class EInventorySlotKind : uint8
{
	/** A bag slot. */
	Bag UMETA(DisplayName = "Bag"),

	/** An equipment slot: the item is being worn. */
	Equipment UMETA(DisplayName = "Equipment")
};

/** An item being dragged out of a bag slot or an equipment slot. */
UCLASS()
class ABILITYSYSTEM_API UInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UItemDataAsset> Item;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	EInventorySlotKind SourceKind = EInventorySlotKind::Bag;

	/** Bag slot index or equipment slot index, by Source Kind. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 SourceIndex = INDEX_NONE;

	/** Items only move within one inventory. */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TWeakObjectPtr<UInventoryComponent> SourceInventory;
};
