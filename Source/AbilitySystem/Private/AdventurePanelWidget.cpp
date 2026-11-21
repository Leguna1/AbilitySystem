#include "AdventurePanelWidget.h"

#include "AdventureSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "AdventurePanel"

namespace
{
	UTextBlock* MakePanelText(UWidgetTree* Tree, const int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());

		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		return Text;
	}
}

/* -------------------- Option handler -------------------- */

void UAdventurePanelOptionHandler::Initialize(UAdventurePanelWidget* InPanel, const int32 InOptionIndex)
{
	Panel = InPanel;
	OptionIndex = InOptionIndex;
}

void UAdventurePanelOptionHandler::HandleClicked()
{
	if (UAdventurePanelWidget* PanelWidget = Panel.Get())
	{
		PanelWidget->ChooseOption(OptionIndex);
	}
}

/* -------------------- Layout -------------------- */

TSharedRef<SWidget> UAdventurePanelWidget::RebuildWidget()
{
	// No Widget Blueprint behind this class: the tree is put together here, once, before Slate is built from it.
	if (!IsValid(WidgetTree))
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}

	if (!IsValid(WidgetTree->RootWidget))
	{
		BuildLayout();
	}

	return Super::RebuildWidget();
}

void UAdventurePanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Game input is off while the panel is open, so the panel itself has to take the keys.
	SetIsFocusable(true);
}

void UAdventurePanelWidget::BuildLayout()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.035f, 0.92f));
	Frame->SetPadding(FMargin(28.0f, 22.0f));

	if (UCanvasPanelSlot* FrameSlot = Canvas->AddChildToCanvas(Frame))
	{
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FrameSlot->SetPosition(FVector2D::ZeroVector);
		FrameSlot->SetAutoSize(true);
	}

	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WidthBox->SetWidthOverride(640.0f);
	Frame->SetContent(WidthBox);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidthBox->SetContent(Column);

	TitleText = MakePanelText(WidgetTree, 26, FLinearColor(1.0f, 0.82f, 0.35f));
	Column->AddChildToVerticalBox(TitleText);

	BodyText = MakePanelText(WidgetTree, 16, FLinearColor(0.9f, 0.9f, 0.9f));

	if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(BodyText))
	{
		BodySlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 16.0f));
	}

	// Long lists (a full bag at the merchant) scroll instead of running off the screen.
	USizeBox* ListBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ListBox->SetMaxDesiredHeight(420.0f);
	Column->AddChildToVerticalBox(ListBox);

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	ListBox->SetContent(Scroll);

	OptionsBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(OptionsBox);

	UTextBlock* HintText = MakePanelText(WidgetTree, 12, FLinearColor(0.55f, 0.55f, 0.6f));
	HintText->SetText(LOCTEXT("Hint", "Click an option or press its number.  Esc closes."));

	if (UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(HintText))
	{
		HintSlot->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 0.0f));
	}
}

/* -------------------- Content -------------------- */

void UAdventurePanelWidget::ShowPanel(const FText& Title, const FText& Body, const TArray<FAdventurePanelOption>& InOptions)
{
	Options = InOptions;

	if (!IsValid(TitleText) || !IsValid(BodyText) || !IsValid(OptionsBox) || !IsValid(WidgetTree))
	{
		return;
	}

	TitleText->SetText(Title);
	BodyText->SetText(Body);
	BodyText->SetVisibility(Body.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	OptionsBox->ClearChildren();
	OptionHandlers.Reset();

	for (int32 Index = 0; Index < Options.Num(); ++Index)
	{
		const FAdventurePanelOption& Option = Options[Index];

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		Button->SetBackgroundColor(Option.bEnabled ? FLinearColor(0.12f, 0.13f, 0.2f, 1.0f) : FLinearColor(0.08f, 0.08f, 0.08f, 0.7f));
		Button->SetIsEnabled(Option.bEnabled);

		// The first nine can be picked with the number keys, so they show their number.
		const FText Label = Index < 9
			? FText::Format(LOCTEXT("NumberedOption", "{0}.  {1}"), Index + 1, Option.Label)
			: Option.Label;

		UTextBlock* LabelText = MakePanelText(WidgetTree, 15, Option.bEnabled ? FLinearColor::White : FLinearColor(0.5f, 0.5f, 0.5f));
		LabelText->SetText(Label);
		Button->SetContent(LabelText);

		UAdventurePanelOptionHandler* Handler = NewObject<UAdventurePanelOptionHandler>(this);
		Handler->Initialize(this, Index);
		Button->OnClicked.AddDynamic(Handler, &UAdventurePanelOptionHandler::HandleClicked);
		OptionHandlers.Add(Handler);

		if (UVerticalBoxSlot* ButtonSlot = OptionsBox->AddChildToVerticalBox(Button))
		{
			ButtonSlot->SetPadding(FMargin(0.0f, 3.0f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		}
	}
}

void UAdventurePanelWidget::FocusPanel()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		SetUserFocus(PlayerController);
	}

	SetKeyboardFocus();
}

void UAdventurePanelWidget::ChooseOption(const int32 OptionIndex)
{
	if (Options.IsValidIndex(OptionIndex) && Options[OptionIndex].bEnabled)
	{
		OnOptionChosen.ExecuteIfBound(OptionIndex);
	}
}

FReply UAdventurePanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// A key still held from the game (the Interact press that opened the panel) repeats; it must not close it again.
	if (InKeyEvent.IsRepeat())
	{
		return FReply::Handled();
	}

	const FKey Key = InKeyEvent.GetKey();
	const UAdventureSettings* Settings = UAdventureSettings::Get();

	if (Key == EKeys::Escape || Key == Settings->InteractKey || Key == Settings->QuestLogKey)
	{
		OnCloseRequested.ExecuteIfBound();
		return FReply::Handled();
	}

	const FKey NumberKeys[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };

	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(NumberKeys)); ++Index)
	{
		if (Key == NumberKeys[Index])
		{
			ChooseOption(Index);
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
