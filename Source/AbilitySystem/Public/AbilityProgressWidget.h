#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "AbilityProgressProvider.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UIBarMaterial.h"
#include "AbilityProgressWidget.generated.h"

class UAbility;
class UAbilityComponent;
class UImage;
class UTextBlock;

/**
 * Standalone bar that displays the active ability's progress, if that ability
 * implements IAbilityProgressProvider.
 *
 * Show/hide is event-driven off the component's activate/end delegates; the
 * fill is refreshed on a ~30hz timer only while shown.
 *
 * Kit cast bar: an Image named BarFill (MI_UI_Bar_Cast) and a Text Block named ProgressText
 * are driven for you: charges fill 0 -> 1, counts (channels like Rapid Fire) drain 1 -> 0, the text
 * reads "Charged Shot  75%". With Manage Visibility on, the widget collapses itself when idle and
 * pops in (fade + slide up) when shown. The events below still fire for anything extra.
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
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/* -------------------- Kit visuals (optional) -------------------- */

	/** Image with the cast bar material. Name it BarFill. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ability|UI|Widgets")
	TObjectPtr<UImage> BarFill;

	/** "Name  Progress". Name it ProgressLabel. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Ability|UI|Widgets")
	TObjectPtr<UTextBlock> ProgressLabel;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Kit")
	FUIBarMaterialSettings BarSettings;

	/** {Name} = the ability, {Progress} = "75%" or "3/8". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Kit")
	FText ProgressTextFormat = NSLOCTEXT("AbilityProgress", "Format", "{Name}  {Progress}");

	/** Count progress (Rapid Fire's shots) drains like a channel. Tick if yours fills instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Kit")
	bool bInvertCount = false;

	/** Collapse when idle, fade and slide in when shown. Off = do it yourself in the events. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Kit")
	bool bManageVisibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Kit", meta = (EditCondition = "bManageVisibility", ClampMin = "0.0", Units = "s"))
	float ShowDuration = 0.12f;

	/** Starts this far below and slides up into place. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|UI|Kit", meta = (EditCondition = "bManageVisibility"))
	float ShowSlideDistance = 12.0f;

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
	void HandleAbilityActivated(FGameplayTag AbilityId, UActiveAbility* Ability);

	UFUNCTION()
	void HandleAbilityEnded(FGameplayTag AbilityId, UActiveAbility* Ability, EAbilityEndReason EndReason);

	void EvaluateActiveAbility();
	void TickFill();
	void Show(EAbilityProgressKind Kind);
	void Hide();

	static FText FormatProgressText(const FAbilityProgress& Progress);

	/** Fill for the kit bar: charges fill, counts drain. */
	float GetBarPercent(const FAbilityProgress& Progress) const;

	void UpdateKitVisuals(const FAbilityProgress& Progress, const FText& Formatted);

	UPROPERTY(Transient)
	FUIBarMaterial BarMaterial;

	/** Seconds into the show animation; < 0 = not animating. */
	float ShowElapsed = -1.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	FTimerHandle FillTimerHandle;

	UPROPERTY(Transient)
	bool bShowing = false;
};