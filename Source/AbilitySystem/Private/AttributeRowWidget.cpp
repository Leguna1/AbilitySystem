#include "AttributeRowWidget.h"

#include "Components/TextBlock.h"

void UAttributeRowWidget::SetLabel(const FText& Label)
{
	if (IsValid(LabelText))
	{
		LabelText->SetText(Label);
	}
}

void UAttributeRowWidget::SetValue(const FText& Value, const bool bIncreased, const bool bDecreased)
{
	if (IsValid(ValueText))
	{
		ValueText->SetText(Value);
	}

	// No flash for the first fill.
	if (bHasValue && (bIncreased || bDecreased))
	{
		OnValueChanged(bIncreased);
	}

	bHasValue = true;
}

void UAttributeRowWidget::SetPreview(const FText& Preview)
{
	if (!IsValid(PreviewText))
	{
		return;
	}

	PreviewText->SetText(Preview);
	PreviewText->SetVisibility(Preview.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
