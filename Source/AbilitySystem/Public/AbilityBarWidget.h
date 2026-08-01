#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilityBarWidget.generated.h"

class UAbility;
class UAbilityComponent;
class UAbilitySlotWidget;
class UPanelWidget;

/**
 * Builds and owns one UAbilitySlotWidget per ability granted on an
 * UAbilityComponent.
 *
 * Setup in the UMG subclass:
 * - Add a container panel (e.g. Horizontal/Vertical Box) and mark it BindWidget
 *   with the name "SlotContainer".
 * - Set SlotWidgetClass to your UAbilitySlotWidget subclass.
 * Then call InitializeBar with the owning component.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UAbilityBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds to the component and (re)builds a slot for every granted ability. */
	UFUNCTION(BlueprintCallable, Category = "Ability|UI")
	void InitializeBar(UAbilityComponent* InAbilityComponent);

	/** Clears and rebuilds all slots from the component's current granted list. */
	UFUNCTION(BlueprintCallable, Category = "Ability|UI")
	void RebuildSlots();

protected:
	virtual void NativeDestruct() override;

	/** Panel that slot widgets are added to. Bind a panel named "SlotContainer" in UMG. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Ability|UI")
	TObjectPtr<UPanelWidget> SlotContainer;

	/** The slot widget class instantiated per ability. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|UI")
	TSubclassOf<UAbilitySlotWidget> SlotWidgetClass;

private:
	void ClearSlots();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAbilitySlotWidget>> SlotWidgets;
};