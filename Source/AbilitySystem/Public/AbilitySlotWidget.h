#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "ResourceComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "AbilitySlotWidget.generated.h"

class UAbility;
class UAbilityComponent;
class UActionBarComponent;
class UActiveAbility;
class UTexture2D;
class UWidget;

/**
 * One action bar slot: icon, name, keybind, cooldown, affordability, active state. May be empty.
 *
 * On an action bar it also edits the bar:
 *   - drag it onto another slot to swap them,
 *   - drag it off the bar, or right-click it, to empty it,
 *   - drop an ability on it (from the skill tree or another slot) to place it here.
 *
 * The C++ base owns the data; the Widget Blueprint owns the look through the events below.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UAbilitySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Points this slot at an ability granted on the component. A null class empties the slot. */
	UFUNCTION(BlueprintCallable, Category = "Ability|UI")
	void InitializeSlot(UAbilityComponent* InAbilityComponent, TSubclassOf<UAbility> InAbilityClass);

	/** Makes this slot SlotIndex of an action bar: its keybind, and drag-and-drop editing. */
	UFUNCTION(BlueprintCallable, Category = "Ability|UI")
	void SetActionBarSlot(UActionBarComponent* InActionBar, int32 InSlotIndex);

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	bool IsEmpty() const;

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	int32 GetSlotIndex() const { return SlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	TSubclassOf<UAbility> GetAbilityClass() const { return AbilityClass; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FGameplayTag GetSlotAbilityId() const { return SlotAbilityId; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FText GetDescription() const { return Description; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	UTexture2D* GetIcon() const { return Icon; }

	/** The action bar slot's label, or the ability's own when not on a bar. */
	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FText GetKeybindLabel() const { return KeybindLabel; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	float GetFocusCost() const { return FocusCost; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	bool IsOnCooldown() const { return bOnCooldown; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	bool CanAfford() const { return bCanAfford; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	bool IsAbilityActive() const { return bIsActive; }

protected:
	virtual void NativeDestruct() override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	/** Right-click empties the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Editing")
	bool bRightClickRemoves = true;

	/** Dragging the ability off the bar and letting go empties the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Editing")
	bool bRemoveWhenDraggedOff = true;

	/** Size of the default drag image (the icon) when Build Drag Visual returns nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Editing")
	FVector2D DragIconSize = FVector2D(48.0f, 48.0f);

	/** Static visuals (icon, name, keybind). Called on init and whenever the slot's ability changes, empty included. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnSlotInitialized();

	/** Show or hide the icon and overlays. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnSlotEmptyChanged(bool bEmpty);

	/** Highlight while something is dragged over this slot. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnDragHoverChanged(bool bHovered);

	/** The widget that follows the cursor while dragging. Null = the icon. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	UWidget* BuildDragVisual();

	/** Return the tooltip widget on hover, or null for none. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	UWidget* BuildTooltipWidget();

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnActiveStateChanged(bool bActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnCooldownStateChanged(bool bOnCooldownNow);

	/** Each poll while on cooldown. Progress: 1 = just started, 0 = ready. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnCooldownUpdated(float Remaining, float Progress);

	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnAffordabilityChanged(bool bCanAffordNow);

private:
	UFUNCTION()
	UWidget* GetTooltipWidget();

	UFUNCTION()
	void HandleAbilityActivated(FGameplayTag AbilityId, UActiveAbility* Ability);

	UFUNCTION()
	void HandleAbilityEnded(FGameplayTag AbilityId, UActiveAbility* Ability, EAbilityEndReason EndReason);

	UFUNCTION()
	void HandleAbilityCommitted(FGameplayTag AbilityId, UActiveAbility* Ability);

	UFUNCTION()
	void HandleResourceChanged(EResourceType ResourceType, EResourceValueType ValueType, float OldValue, float NewValue);

	void Bind();
	void Unbind();
	void ResetRuntimeState();

	void SetActive(bool bNewActive);
	void BeginCooldownPoll();
	void StopCooldownPoll();
	void TickCooldown();
	void RefreshAffordability();
	void SetCooldownState(bool bNewOnCooldown);
	void SetAffordable(bool bNewCanAfford);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<UActionBarComponent> ActionBar;

	UPROPERTY(Transient)
	TSubclassOf<UAbility> AbilityClass;

	int32 SlotIndex = INDEX_NONE;

	FGameplayTag SlotAbilityId;
	FText DisplayName;
	FText Description;
	FText KeybindLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Icon;

	float FocusCost = 0.0f;
	bool bOnCooldown = false;
	bool bCanAfford = true;
	bool bIsActive = false;

	/** Cooldown sweep poll rate in seconds. Runs only while on cooldown. */
	UPROPERTY(EditDefaultsOnly, Category = "Ability|UI", meta = (AllowPrivateAccess = "true"))
	float CooldownPollInterval = 0.05f;

	FTimerHandle CooldownTimerHandle;
};
