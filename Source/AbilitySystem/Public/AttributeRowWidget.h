#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttributeRowWidget.generated.h"

class UTextBlock;

/**
 * One line of an Attribute Panel: "Attack Power   42 – 55   (+2)".
 * The panel fills it; the Widget Blueprint only lays it out.
 * Optional widgets: LabelText, ValueText, PreviewText (the "+2" shown while hovering a core stat).
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UAttributeRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLabel(const FText& Label);

	/** New value. bIncreased/bDecreased tell the Blueprint to flash it (a buff landed, a point was spent). */
	void SetValue(const FText& Value, bool bIncreased, bool bDecreased);

	/** Empty text hides the preview. */
	void SetPreview(const FText& Preview);

protected:
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Attribute Row")
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Attribute Row")
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Attribute Row")
	TObjectPtr<UTextBlock> PreviewText;

	/** The value went up or down. A green or red flash, a little pop. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attribute Row")
	void OnValueChanged(bool bIncreased);

private:
	bool bHasValue = false;
};
