#include "CompanionSlotWidget.h"

UCompanionSlotWidget::UCompanionSlotWidget()
{
	bRightClickRemoves = false;
	bRemoveWhenDraggedOff = false;
}

FReply UCompanionSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// No commands, no rearranging: swallow the click so nothing behind the bar gets it either.
	return FReply::Handled();
}

bool UCompanionSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	return false;
}
