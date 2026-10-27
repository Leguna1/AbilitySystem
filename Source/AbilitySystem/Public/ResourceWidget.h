#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ResourceComponent.h"
#include "UIBarMaterial.h"
#include "ResourceWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;

/**
 * Displays one resource (Health, Focus) of a Resource Component.
 *
 * Either a kit bar: an Image named BarFill using a bar material (Percent, Trail, LowPulse),
 * or a plain Progress Bar named ResourceProgressBar. Both may exist.
 * Pick the resource and the bar material per instance in the Details panel.
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

	UFUNCTION(BlueprintPure, Category = "Resource Bar")
	UResourceComponent* GetResourceComponent() const { return ResourceComponent; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** The bar material and its trail / low-pulse behaviour. Health: Low Pulse Threshold 0.25. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Resource Bar")
	FUIBarMaterialSettings BarSettings;

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
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Resource Bar|Widgets")
	TObjectPtr<UProgressBar> ResourceProgressBar;

	/** Optional kit bar: an Image with the bar material. Name it BarFill. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Resource Bar|Widgets")
	TObjectPtr<UImage> BarFill;

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

	UPROPERTY(Transient)
	FUIBarMaterial BarMaterial;

	/** The next refresh snaps (no damage trail): first fill, or a different component. */
	bool bSnapNextRefresh = true;

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