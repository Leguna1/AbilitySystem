#include "AdventureHUDWidget.h"

#include "AdventureComponent.h"
#include "AdventureSettings.h"
#include "BaseCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CurrencyDataAsset.h"
#include "Engine/World.h"
#include "QuestLogComponent.h"
#include "ResourceComponent.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "AdventureHUD"

namespace
{
	constexpr double TrackerRefreshInterval = 0.25;

	UTextBlock* MakeHUDText(UWidgetTree* Tree, const int32 FontSize, const FLinearColor& Color, const bool bCentered)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());

		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));

		// Readable over any background without a frame behind every line.
		Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));
		Text->SetJustification(bCentered ? ETextJustify::Center : ETextJustify::Left);
		return Text;
	}

	UCanvasPanelSlot* PlaceOnCanvas(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Anchor, const FVector2D& Offset)
	{
		UCanvasPanelSlot* CanvasSlot = Canvas->AddChildToCanvas(Widget);

		if (CanvasSlot)
		{
			// Anchor and alignment are the same point, so Offset is measured from that screen edge or corner.
			CanvasSlot->SetAnchors(FAnchors(static_cast<float>(Anchor.X), static_cast<float>(Anchor.Y)));
			CanvasSlot->SetAlignment(Anchor);
			CanvasSlot->SetPosition(Offset);
			CanvasSlot->SetAutoSize(true);
		}

		return CanvasSlot;
	}

	void SetTextIfChanged(UTextBlock* TextBlock, const FText& NewText)
	{
		if (IsValid(TextBlock) && !TextBlock->GetText().EqualTo(NewText))
		{
			TextBlock->SetText(NewText);
		}
	}

	void SetHUDPartShown(UWidget* Widget, const bool bShown)
	{
		if (IsValid(Widget))
		{
			const ESlateVisibility Wanted = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

			if (Widget->GetVisibility() != Wanted)
			{
				Widget->SetVisibility(Wanted);
			}
		}
	}
}

/* -------------------- Layout -------------------- */

void UAdventureHUDWidget::SetAdventure(UAdventureComponent* InAdventure)
{
	Adventure = InAdventure;

	// The HUD only shows things; clicks go through it to the game.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

TSharedRef<SWidget> UAdventureHUDWidget::RebuildWidget()
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

void UAdventureHUDWidget::BuildLayout()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	WidgetTree->RootWidget = Canvas;

	/* Boss: top center. */
	BossBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PlaceOnCanvas(Canvas, BossBox, FVector2D(0.5, 0.0), FVector2D(0.0, 28.0));

	BossNameText = MakeHUDText(WidgetTree, 20, FLinearColor(1.0f, 0.45f, 0.35f), true);
	BossBox->AddChildToVerticalBox(BossNameText);

	USizeBox* BossBarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BossBarSize->SetWidthOverride(620.0f);
	BossBarSize->SetHeightOverride(16.0f);
	BossBox->AddChildToVerticalBox(BossBarSize);

	BossHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	BossHealthBar->SetFillColorAndOpacity(FLinearColor(0.8f, 0.08f, 0.05f));
	BossBarSize->SetContent(BossHealthBar);

	/* Banner: below the boss bar. */
	BannerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PlaceOnCanvas(Canvas, BannerBox, FVector2D(0.5, 0.0), FVector2D(0.0, 110.0));

	BannerTitleText = MakeHUDText(WidgetTree, 34, FLinearColor(1.0f, 0.86f, 0.45f), true);
	BannerBox->AddChildToVerticalBox(BannerTitleText);

	BannerSubtitleText = MakeHUDText(WidgetTree, 16, FLinearColor(0.9f, 0.9f, 0.9f), true);
	BannerBox->AddChildToVerticalBox(BannerSubtitleText);

	/* Tracker: right edge. */
	TrackerFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	TrackerFrame->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
	TrackerFrame->SetPadding(FMargin(12.0f, 10.0f));
	PlaceOnCanvas(Canvas, TrackerFrame, FVector2D(1.0, 0.0), FVector2D(-24.0, 180.0));

	USizeBox* TrackerSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	TrackerSize->SetWidthOverride(300.0f);
	TrackerFrame->SetContent(TrackerSize);

	TrackerText = MakeHUDText(WidgetTree, 13, FLinearColor(0.92f, 0.92f, 0.92f), false);
	TrackerText->SetAutoWrapText(true);
	TrackerSize->SetContent(TrackerText);

	/* Feed: left edge, vertically centered. */
	FeedText = MakeHUDText(WidgetTree, 14, FLinearColor(0.75f, 0.95f, 0.75f), false);
	PlaceOnCanvas(Canvas, FeedText, FVector2D(0.0, 0.5), FVector2D(24.0, 0.0));

	/* Prompt and cast bar: above the ability bar. */
	PromptText = MakeHUDText(WidgetTree, 18, FLinearColor::White, true);
	PlaceOnCanvas(Canvas, PromptText, FVector2D(0.5, 1.0), FVector2D(0.0, -230.0));

	ChannelBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PlaceOnCanvas(Canvas, ChannelBox, FVector2D(0.5, 1.0), FVector2D(0.0, -270.0));

	ChannelText = MakeHUDText(WidgetTree, 15, FLinearColor(0.8f, 0.9f, 1.0f), true);
	ChannelBox->AddChildToVerticalBox(ChannelText);

	USizeBox* ChannelBarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ChannelBarSize->SetWidthOverride(360.0f);
	ChannelBarSize->SetHeightOverride(12.0f);
	ChannelBox->AddChildToVerticalBox(ChannelBarSize);

	ChannelBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	ChannelBar->SetFillColorAndOpacity(FLinearColor(0.35f, 0.7f, 1.0f));
	ChannelBarSize->SetContent(ChannelBar);

	/* Death: center. */
	CenterBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	PlaceOnCanvas(Canvas, CenterBox, FVector2D(0.5, 0.5), FVector2D(0.0, -60.0));

	CenterTitleText = MakeHUDText(WidgetTree, 46, FLinearColor(0.85f, 0.1f, 0.1f), true);
	CenterBox->AddChildToVerticalBox(CenterTitleText);

	CenterSubtitleText = MakeHUDText(WidgetTree, 18, FLinearColor(0.9f, 0.9f, 0.9f), true);
	CenterBox->AddChildToVerticalBox(CenterSubtitleText);

	// Everything starts hidden; Refresh HUD shows what has something to say.
	SetHUDPartShown(BossBox, false);
	SetHUDPartShown(BannerBox, false);
	SetHUDPartShown(TrackerFrame, false);
	SetHUDPartShown(PromptText, false);
	SetHUDPartShown(ChannelBox, false);
	SetHUDPartShown(CenterBox, false);
}

/* -------------------- Refresh -------------------- */

void UAdventureHUDWidget::RefreshHUD()
{
	const UAdventureComponent* Hub = Adventure.Get();

	if (!IsValid(Hub) || !IsValid(BannerBox))
	{
		return;
	}

	/* Banner */
	FText BannerTitle;
	FText BannerSubtitle;
	const bool bBanner = Hub->GetBanner(BannerTitle, BannerSubtitle);

	SetHUDPartShown(BannerBox, bBanner);

	if (bBanner)
	{
		SetTextIfChanged(BannerTitleText, BannerTitle);
		SetTextIfChanged(BannerSubtitleText, BannerSubtitle);
	}

	/* Boss */
	const ABaseCharacter* Boss = Hub->GetActiveBoss();
	const UResourceComponent* BossResources = IsValid(Boss) && !Boss->IsDead() ? Boss->GetResourceComponent() : nullptr;

	SetHUDPartShown(BossBox, IsValid(BossResources));

	if (IsValid(BossResources))
	{
		const float Health = BossResources->GetResourceValue(EResourceType::Health, EResourceValueType::Current);
		const float MaxHealth = BossResources->GetResourceValue(EResourceType::Health, EResourceValueType::Max);

		SetTextIfChanged(BossNameText, Boss->GetCharacterDisplayName());
		BossHealthBar->SetPercent(MaxHealth > 0.0f ? FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f) : 0.0f);
	}

	/* Feed */
	FString Feed;

	for (const FAdventureNotification& Notification : Hub->GetNotifications())
	{
		if (!Feed.IsEmpty())
		{
			Feed += TEXT("\n");
		}

		Feed += Notification.Text.ToString();
	}

	SetTextIfChanged(FeedText, FText::FromString(Feed));

	/* Prompt */
	const FText& Prompt = Hub->GetFocusPrompt();
	const bool bPrompt = !Prompt.IsEmpty() && !Hub->IsChanneling() && !Hub->IsRespawning();

	SetHUDPartShown(PromptText, bPrompt);

	if (bPrompt)
	{
		const bool bUsable = Hub->CanInteractWithFocus();

		SetTextIfChanged(PromptText, bUsable
			? FText::Format(LOCTEXT("PromptUsable", "[{0}]  {1}"), UAdventureSettings::Get()->InteractKey.GetDisplayName(), Prompt)
			: Prompt);

		PromptText->SetColorAndOpacity(FSlateColor(bUsable ? FLinearColor::White : FLinearColor(0.7f, 0.7f, 0.7f)));
	}

	/* Channel */
	SetHUDPartShown(ChannelBox, Hub->IsChanneling());

	if (Hub->IsChanneling())
	{
		SetTextIfChanged(ChannelText, Hub->GetChannelLabel());
		ChannelBar->SetPercent(Hub->GetChannelProgress());
	}

	/* Death */
	SetHUDPartShown(CenterBox, Hub->IsRespawning());

	if (Hub->IsRespawning())
	{
		SetTextIfChanged(CenterTitleText, LOCTEXT("Died", "You have fallen"));
		SetTextIfChanged(CenterSubtitleText, FText::Format(LOCTEXT("Respawning", "Waking in {0}..."),
			FMath::CeilToInt(Hub->GetRespawnTimeRemaining())));
	}

	/* Tracker: rebuilt a few times a second, it is the only part that costs anything to put together. */
	const UWorld* World = GetWorld();
	const double Now = IsValid(World) ? World->GetTimeSeconds() : 0.0;

	if (Now >= NextTrackerRefreshTime)
	{
		NextTrackerRefreshTime = Now + TrackerRefreshInterval;
		RefreshTracker();
	}
}

void UAdventureHUDWidget::RefreshTracker()
{
	const UAdventureComponent* Hub = Adventure.Get();

	if (!IsValid(Hub))
	{
		return;
	}

	FString Lines;

	if (const UQuestLogComponent* QuestLog = Hub->GetQuestLog())
	{
		Lines = QuestLog->BuildTrackerText().ToString();
	}

	if (const UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		const int32 Total = Progress->GetObjectives().Num();

		if (Total > 0)
		{
			if (!Lines.IsEmpty())
			{
				Lines += TEXT("\n\n");
			}

			Lines += Progress->IsVictoryAchieved()
				? FString(TEXT("Light has returned to the world."))
				: FString::Printf(TEXT("Altars and guardians: %d/%d"), Progress->GetCompletedObjectiveCount(), Total);
		}
	}

	if (IsValid(Hub->GetGoldCurrency()))
	{
		if (!Lines.IsEmpty())
		{
			Lines += TEXT("\n");
		}

		Lines += FString::Printf(TEXT("Gold: %d"), Hub->GetGold());
	}

	SetHUDPartShown(TrackerFrame, !Lines.IsEmpty());
	SetTextIfChanged(TrackerText, FText::FromString(Lines));
}

#undef LOCTEXT_NAMESPACE
