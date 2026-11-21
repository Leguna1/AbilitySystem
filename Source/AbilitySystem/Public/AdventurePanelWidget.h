#pragma once

#include "CoreMinimal.h"
#include "AdventureTypes.h"
#include "Blueprint/UserWidget.h"
#include "AdventurePanelWidget.generated.h"

class UAdventurePanelWidget;
class UTextBlock;
class UVerticalBox;

/** Remembers which option a button stands for, since a button's click carries no information. */
UCLASS()
class ABILITYSYSTEM_API UAdventurePanelOptionHandler : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UAdventurePanelWidget* InPanel, int32 InOptionIndex);

	UFUNCTION()
	void HandleClicked();

private:
	TWeakObjectPtr<UAdventurePanelWidget> Panel;
	int32 OptionIndex = INDEX_NONE;
};

/**
 * The window villagers, waystones and the quest log talk through: a title, some text, a list of options.
 * Built entirely in C++, so there is no Widget Blueprint to make. Shown by the Adventure Component (Open Panel).
 *
 * Mouse: click an option. Keys: 1-9 pick an option, Esc or the Interact key closes.
 */
UCLASS()
class ABILITYSYSTEM_API UAdventurePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Replaces everything the panel shows. */
	void ShowPanel(const FText& Title, const FText& Body, const TArray<FAdventurePanelOption>& InOptions);

	/** Takes keyboard focus, so the number keys and Esc reach the panel. */
	void FocusPanel();

	/** An enabled option was clicked or its number pressed. */
	void ChooseOption(int32 OptionIndex);

	FAdventurePanelChoiceDelegate OnOptionChosen;
	FSimpleDelegate OnCloseRequested;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void BuildLayout();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> OptionsBox;

	/** One per option button; kept here so they live as long as their buttons. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAdventurePanelOptionHandler>> OptionHandlers;

	TArray<FAdventurePanelOption> Options;
};
