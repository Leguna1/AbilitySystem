#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResourceComponent.h"
#include "ResourceWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * Displays one resource from UMyResourceComponent.
 *
 * Create a Widget Blueprint child and assign DisplayedResource
 * to Health or Focus in Class Defaults.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UResourceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * Manually assigns a resource component.
	 *
	 * Normally the widget automatically finds the component on the
	 * owning player's pawn during NativeConstruct.
	 */
	UFUNCTION(BlueprintCallable, Category = "Resource Bar")
	void SetResourceComponent(UResourceComponent* NewResourceComponent);

	/**
	 * Changes which resource this widget displays.
	 */
	UFUNCTION(BlueprintCallable, Category = "Resource Bar")
	void SetDisplayedResource(EResourceType NewResourceType);

	/**
	 * Immediately refreshes the bar using the current resource values.
	 */
	UFUNCTION(BlueprintCallable, Category = "Resource Bar")
	void RefreshResourceBar();

	UFUNCTION(BlueprintPure, Category = "Resource Bar")
	EResourceType GetDisplayedResource() const
	{
		return DisplayedResource;
	}

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/* -------------------- Designer Configuration -------------------- */

	/**
	 * Select Health or Focus in the Widget Blueprint Class Defaults.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Bar")
	EResourceType DisplayedResource = EResourceType::Health;

	/**
	 * Automatically finds UMyResourceComponent on the owning player's pawn.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Bar")
	bool bFindResourceComponentAutomatically = true;

	/**
	 * Displays "Current / Max" when ResourceValueText exists.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Bar|Text")
	bool bShowValueText = true;

	/**
	 * Number of decimal places shown in the resource text.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Bar|Text", meta = (ClampMin = "0", ClampMax = "3", EditCondition = "bShowValueText"))
	int32 DecimalPlaces = 0;

	/* -------------------- Bound Designer Widgets -------------------- */

	/**
	 * The Progress Bar in the Widget Blueprint must be named:
	 * ResourceProgressBar
	 */
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Resource Bar|Widgets")
	TObjectPtr<UProgressBar> ResourceProgressBar;

	/**
	 * Optional Text Block.
	 *
	 * Name it ResourceValueText in the Widget Blueprint.
	 */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Resource Bar|Widgets")
	TObjectPtr<UTextBlock> ResourceValueText;

	/**
	 * Called after the resource bar has refreshed.
	 *
	 * Useful for Blueprint-specific effects, colors or animations.
	 */
	UFUNCTION(
		BlueprintImplementableEvent,
		Category = "Resource Bar"
	)
	void OnResourceBarRefreshed(float CurrentValue, float MaxValue, float Percent);

private:
	UPROPERTY(Transient)
	TObjectPtr<UResourceComponent> ResourceComponent;

	UFUNCTION()
	void HandleResourceChanged(
		EResourceType ResourceType,
		EResourceValueType ValueType,
		float OldValue,
		float NewValue
	);

	void FindOwningResourceComponent();
	void BindToResourceComponent();
	void UnbindFromResourceComponent();

	FText MakeResourceValueText(
		float CurrentValue,
		float MaxValue
	) const;
};