#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ActiveEffectsBarWidget.generated.h"

class UCombatantComponent;
class UActiveEffectWidget;
class UPanelWidget;

/**
 * Shows the active effects on a character: buffs, procs and statuses.
 * Rebuilds on OnEffectsChanged and polls countdowns while any effect is timed.
 *
 * Setup: a panel named "EffectContainer" (BindWidget), EffectWidgetClass set,
 * then call InitializeEffectsBar with the character's combatant component.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UActiveEffectsBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Effect|UI")
	void InitializeEffectsBar(UCombatantComponent* InCombatantComponent);

protected:
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Effect|UI")
	TObjectPtr<UPanelWidget> EffectContainer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect|UI")
	TSubclassOf<UActiveEffectWidget> EffectWidgetClass;

	/** Countdown refresh rate while timed effects are showing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect|UI", meta = (ClampMin = "0.02"))
	float UpdateInterval = 0.1f;

private:
	UFUNCTION()
	void HandleEffectsChanged();

	void SyncEffects();

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActiveEffectWidget>> EffectWidgets;

	FTimerHandle UpdateTimerHandle;
};