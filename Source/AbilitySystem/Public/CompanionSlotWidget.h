#pragma once

#include "CoreMinimal.h"
#include "AbilitySlotWidget.h"
#include "CompanionSlotWidget.generated.h"

class UCompanionComponent;

/**
 * A slot on the companion bar. Same look and cooldown sweep as your own slots, plus:
 *   - left-click: the companion uses it (on your target),
 *   - right-click: toggle auto-cast (shown through On Auto Cast Changed),
 *   - drag onto another companion slot: rearrange.
 * Use it as the Slot Widget Class of the companion's bar widget.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UCompanionSlotWidget : public UAbilitySlotWidget
{
	GENERATED_BODY()

public:
	UCompanionSlotWidget();

	UFUNCTION(BlueprintPure, Category = "Companion|UI")
	bool IsAutoCast() const;

protected:
	virtual void NativeDestruct() override;
	virtual void NativeOnSlotChanged() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	/** Show or hide the auto-cast glow. Also called whenever the slot's ability changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Companion|UI")
	void OnAutoCastChanged(bool bAutoCast);

private:
	UFUNCTION()
	void HandleAutoCastChanged(FGameplayTag AbilityId, bool bAutoCast);

	/** The leader's Companion Component: the companion's owner (the leader). */
	UCompanionComponent* FindCompanionComponent() const;

	void BindCompanionComponent();

	TWeakObjectPtr<UCompanionComponent> BoundCompanionComponent;

	/** A left press that may still become a click (use) rather than a drag. */
	bool bPendingClick = false;
};
