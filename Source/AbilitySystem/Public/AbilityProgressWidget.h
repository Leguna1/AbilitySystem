#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "AbilityProgressProvider.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "AbilityProgressWidget.generated.h"

class UAbility;
class UAbilityComponent;

/**
 * Standalone bar that displays the active ability's progress, if that ability
 * implements IAbilityProgressProvider.
 *
 * Show/hide is event-driven off the component's activate/end delegates; the
 * fill is refreshed on a ~30hz timer only while shown.
 *
 * Build the visuals in a UMG subclass by implementing the events below.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UAbilityProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds to the component's activate/end events. Call once after creation. */
	UFUNCTION(BlueprintCallable, Category = "Ability|UI")
	void InitializeProgress(UAbilityComponent* InAbilityComponent);

	UFUNCTION(BlueprintPure, Category = "Ability|UI")
	bool IsShowing() const { return bShowing; }

protected:
	virtual void NativeDestruct() override;

	/**
	 * Called when the bar should appear for an ability that reports progress.
	 * Make your root visible here.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnProgressShown(EAbilityProgressKind Kind);

	/** Called when the bar should disappear. Hide your root here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnProgressHidden();

	/**
	 * Called each fill tick while shown. Set your ProgressBar percent to
	 * Normalized and your label to ProgressText.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|UI")
	void OnProgressUpdated(const FAbilityProgress& Progress, const FText& ProgressText);

	/** Frequency of the fill refresh, in seconds. Default ~30hz. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|UI")
	float UpdateInterval = 1.0f / 30.0f;

private:
	UFUNCTION()
	void HandleAbilityActivated(FGameplayTag AbilityId, UAbility* Ability);

	UFUNCTION()
	void HandleAbilityEnded(FGameplayTag AbilityId, UAbility* Ability, EAbilityEndReason EndReason);

	void EvaluateActiveAbility();
	void TickFill();
	void Show(EAbilityProgressKind Kind);
	void Hide();

	static FText FormatProgressText(const FAbilityProgress& Progress);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	FTimerHandle FillTimerHandle;

	UPROPERTY(Transient)
	bool bShowing = false;
};