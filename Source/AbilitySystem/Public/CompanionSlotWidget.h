#pragma once

#include "CoreMinimal.h"
#include "AbilitySlotWidget.h"
#include "CompanionSlotWidget.generated.h"

/**
 * A slot on the companion bar: shows one of the companion's abilities and its cooldown, like your own slots.
 * Display only: the companion decides for itself, so clicks, drags and drops do nothing.
 * Use it as the Slot Widget Class of the companion's bar widget.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UCompanionSlotWidget : public UAbilitySlotWidget
{
	GENERATED_BODY()

public:
	UCompanionSlotWidget();

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
};
