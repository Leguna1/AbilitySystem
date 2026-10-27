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
 *
 * Several bars can share one action bar: give each its Slot Indices (left 0-4, the mouse pod 10-11...).
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

	/** Per instance, so the same bar can use normal, mouse (LMB/RMB) or pet slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI")
	TSubclassOf<UAbilitySlotWidget> SlotWidgetClass;

	/**
	 * Show the owner's Action Bar Component when it has one. Off = always one slot per granted
	 * ability (the pet: its abilities, whatever bar it may still carry).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI")
	bool bUseActionBar = true;

	/** The action bar slots this bar shows, in order. Empty = all of them. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI")
	TArray<int32> SlotIndices;

	/** Without an action bar (the companion): show at most this many abilities. 0 = all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI", meta = (ClampMin = "0"))
	int32 MaxGrantedSlots = 0;

	/** Without an action bar: always show this many slots, filling the rest with empty ones (the pet's 4). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI", meta = (ClampMin = "0"))
	int32 MinGrantedSlots = 0;

	/** Gap between neighbouring slots, in Horizontal or Vertical Boxes. None before the first or after the last. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI", meta = (ClampMin = "0.0"))
	float SlotSpacing = 4.0f;

private:
	UFUNCTION()
	void HandleActionBarChanged(int32 SlotIndex);

	UFUNCTION()
	void HandleGrantedAbilitiesChanged();

	void RefreshSlot(int32 WidgetIndex);

	/** The action bar slots shown, in order: Slot Indices, or every slot. */
	void GetShownActionBarSlots(TArray<int32>& OutSlots) const;
	void AddSlotWidget(UAbilitySlotWidget* AbilitySlot);
	void ClearSlots();
	void Unbind();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<UActionBarComponent> ActionBar;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAbilitySlotWidget>> SlotWidgets;

	/** Action bar slot index of each slot widget. */
	TArray<int32> ShownSlotIndices;
};
