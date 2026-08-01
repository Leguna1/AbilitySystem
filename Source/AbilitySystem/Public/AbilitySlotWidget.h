#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "ResourceComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "AbilitySlotWidget.generated.h"

class UAbility;
class UAbilityComponent;
class UTexture2D;
class UWidget;

/**
 * Displays a single granted ability: icon, display name, keybind, and live
 * active/inactive state.
 *
 * The C++ base owns the data plumbing. Bind the visuals in a UMG subclass:
 * override the BlueprintImplementableEvents below to push values into your
 * widgets, or read the BlueprintReadOnly getters directly in the graph.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UAbilitySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Points this slot at an ability class granted on the given component.
	 * Reads display data from the class default object; no instance required.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ability|UI")
	void InitializeSlot(UAbilityComponent* InAbilityComponent, TSubclassOf<UAbility> InAbilityClass);

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FGameplayTag GetSlotAbilityId() const { return SlotAbilityId; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FText GetDescription() const { return Description; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	UTexture2D* GetIcon() const { return Icon; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FGameplayTag GetActivationInputTag() const { return ActivationInputTag; }

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	FText GetKeybindLabel() const { return KeybindLabel; }

	/** Focus cost read from the ability's defaults. Static; set once at init. */
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

	/** Called once after InitializeSlot resolves the display data. Populate static visuals here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnSlotInitialized();

	/**
	 * Return the widget to show as this slot's tooltip on hover.
	 * Build a tooltip widget here and read GetDisplayName/GetDescription/GetKeybindLabel
	 * to fill it. Return null for no tooltip.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	UWidget* BuildTooltipWidget();

	/** Called whenever this ability becomes the active ability or stops being active. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnActiveStateChanged(bool bActive);

	/** Called when the ability enters or leaves cooldown. Toggle your sweep overlay here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnCooldownStateChanged(bool bOnCooldownNow);

	/**
	 * Called each poll tick while on cooldown. Drive your radial/linear sweep from
	 * Progress (1 = just started, 0 = ready) and show Remaining seconds if desired.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnCooldownUpdated(float Remaining, float Progress);

	/** Called when affordability changes. Grey out / restore the slot here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnAffordabilityChanged(bool bCanAffordNow);

private:
	UFUNCTION()
	UWidget* GetTooltipWidget();

	UFUNCTION()
	void HandleAbilityActivated(FGameplayTag AbilityId, UAbility* Ability);

	UFUNCTION()
	void HandleAbilityEnded(FGameplayTag AbilityId, UAbility* Ability, EAbilityEndReason EndReason);

	UFUNCTION()
	void HandleAbilityCommitted(FGameplayTag AbilityId, UAbility* Ability);

	UFUNCTION()
	void HandleResourceChanged(EResourceType ResourceType, EResourceValueType ValueType, float OldValue, float NewValue);

	void SetActive(bool bNewActive);

	void BeginCooldownPoll();
	void TickCooldown();
	void RefreshAffordability();

	void SetCooldownState(bool bNewOnCooldown);
	void SetAffordable(bool bNewCanAfford);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TSubclassOf<UAbility> AbilityClass;

	UPROPERTY(Transient)
	FGameplayTag SlotAbilityId;

	UPROPERTY(Transient)
	FText DisplayName;

	UPROPERTY(Transient)
	FText Description;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(Transient)
	FGameplayTag ActivationInputTag;

	UPROPERTY(Transient)
	FText KeybindLabel;

	UPROPERTY(Transient)
	float FocusCost = 0.0f;

	UPROPERTY(Transient)
	bool bOnCooldown = false;

	UPROPERTY(Transient)
	bool bCanAfford = true;

	/** Cooldown sweep poll rate in seconds. Runs only while on cooldown. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|UI", meta = (AllowPrivateAccess = "true"))
	float CooldownPollInterval = 0.1f;

	FTimerHandle CooldownTimerHandle;

	UPROPERTY(Transient)
	bool bIsActive = false;
};