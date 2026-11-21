#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AdventureHUDWidget.generated.h"

class UAdventureComponent;
class UBorder;
class UCanvasPanel;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * The game layer's HUD, built entirely in C++ (no Widget Blueprint to make):
 *   top:     zone / event banner, boss name and health
 *   right:   quest tracker, world progress, gold
 *   left:    message feed ("+120 experience", "Quest accepted")
 *   bottom:  interaction prompt ("[F] Open chest"), cast bar for channels
 *   center:  death countdown
 *
 * Created and refreshed by the Adventure Component; it never takes mouse clicks.
 * It sits next to your own HUD (player bar, ability bar) and doesn't replace it.
 */
UCLASS()
class ABILITYSYSTEM_API UAdventureHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetAdventure(UAdventureComponent* InAdventure);

	/** Reads everything from the Adventure Component and shows it. Called every frame by that component. */
	void RefreshHUD();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildLayout();
	void RefreshTracker();

	TWeakObjectPtr<UAdventureComponent> Adventure;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BannerBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BannerTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BannerSubtitleText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> BossBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> BossHealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TrackerFrame;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrackerText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FeedText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromptText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ChannelBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChannelText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ChannelBar;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> CenterBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CenterTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CenterSubtitleText;

	double NextTrackerRefreshTime = 0.0;
};
