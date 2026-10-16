#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilityBarWidget.generated.h"

class UAbilityComponent;
class UAbilitySlotWidget;
class UActionBarComponent;
class UPanelWidget;

/**
 * Shows an Action Bar Component: one slot widget per key, empty or not.
 * Put one on the HUD and another inside the skill tree: both show and edit the same bar.
 *
 * Setup in the Widget Blueprint:
 * - A Horizontal or Vertical Box named "SlotContainer".
 * - Slot Widget Class set to your Ability Slot Widget Blueprint.
 * Then call Initialize Bar with the player's Ability Component.
 *
 * Without an Action Bar Component on the owner, it falls back to one slot per granted ability (read-only).
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UAbilityBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Ability|UI")
	void InitializeBar(UAbilityComponent* InAbilityComponent);

	/** Clears and rebuilds every slot. */
	UFUNCTION(BlueprintCallable, Category = "Ability|UI")
	void RebuildSlots();

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Ability|UI")
	TObjectPtr<UPanelWidget> SlotContainer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|UI")
	TSubclassOf<UAbilitySlotWidget> SlotWidgetClass;

	/** Gap between neighbouring slots, in Horizontal or Vertical Boxes. None before the first or after the last. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI", meta = (ClampMin = "0.0"))
	float SlotSpacing = 4.0f;

private:
	UFUNCTION()
	void HandleActionBarChanged(int32 SlotIndex);

	UFUNCTION()
	void HandleGrantedAbilitiesChanged();

	void RefreshSlot(int32 SlotIndex);
	void AddSlotWidget(UAbilitySlotWidget* AbilitySlot);
	void ClearSlots();
	void Unbind();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<UActionBarComponent> ActionBar;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAbilitySlotWidget>> SlotWidgets;
};
