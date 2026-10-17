#include "CoreStatRowWidget.h"

#include "AttributePanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UCoreStatRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(AddButton))
	{
		AddButton->OnClicked.AddUniqueDynamic(this, &UCoreStatRowWidget::HandleAddClicked);
	}
}

void UCoreStatRowWidget::Setup(UAttributePanelWidget* InPanel, const FGameplayTag InCoreStat, const FText& Label, const FText& InDescription)
{
	Panel = InPanel;
	CoreStat = InCoreStat;
	Description = InDescription;

	if (IsValid(LabelText))
	{
		LabelText->SetText(Label);
	}

	SetToolTipText(InDescription);
}

void UCoreStatRowWidget::Refresh(const float Value, const bool bCanSpend, const bool bSpendingAllowed)
{
	if (IsValid(ValueText))
	{
		ValueText->SetText(FText::AsNumber(FMath::RoundToInt(Value)));
	}

	if (IsValid(AddButton))
	{
		AddButton->SetVisibility(bSpendingAllowed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		AddButton->SetIsEnabled(bCanSpend);
	}

	OnSpendStateChanged(bCanSpend, bSpendingAllowed);
}

void UCoreStatRowWidget::SpendPoint()
{
	if (UAttributePanelWidget* Owner = Panel.Get())
	{
		Owner->SpendPoint(CoreStat);
	}
}

void UCoreStatRowWidget::HandleAddClicked()
{
	SpendPoint();
}

void UCoreStatRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (UAttributePanelWidget* Owner = Panel.Get())
	{
		Owner->SetPreviewCoreStat(CoreStat);
	}
}

void UCoreStatRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	if (UAttributePanelWidget* Owner = Panel.Get())
	{
		Owner->SetPreviewCoreStat(FGameplayTag());
	}
}
