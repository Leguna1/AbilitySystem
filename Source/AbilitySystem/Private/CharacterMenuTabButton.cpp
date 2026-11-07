#include "CharacterMenuTabButton.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

void UCharacterMenuTabButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (IsValid(TabButton))
	{
		TabButton->OnClicked.AddUniqueDynamic(this, &UCharacterMenuTabButton::HandleClicked);
	}
}

void UCharacterMenuTabButton::SetupTab(const int32 InTabIndex, const FText& InLabel)
{
	AssignedTabIndex = InTabIndex;

	if (IsValid(LabelText))
	{
		LabelText->SetText(InLabel);
	}

	// Start unselected so the colour is right before the menu picks a tab.
	bIsSelected = true;
	SetSelected(false);
}

void UCharacterMenuTabButton::SetKeyText(const FText& InKeyText)
{
	if (!IsValid(KeyText))
	{
		return;
	}

	KeyText->SetText(InKeyText);
	KeyText->SetVisibility(InKeyText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UCharacterMenuTabButton::SetSelected(const bool bInSelected)
{
	if (bIsSelected == bInSelected)
	{
		return;
	}

	bIsSelected = bInSelected;

	if (IsValid(LabelText))
	{
		LabelText->SetColorAndOpacity(bIsSelected ? SelectedLabelColor : NormalLabelColor);
	}

	OnSelectionChanged(bIsSelected);
}

void UCharacterMenuTabButton::HandleClicked()
{
	OnTabClicked.Broadcast(AssignedTabIndex);
}
