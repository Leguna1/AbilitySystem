#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterMenuTabButton.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCharacterMenuTabClickedSignature, int32, ClickedTabIndex);

/**
 * One tab in the Character Menu's tab bar ("Character  [I]"). The menu creates one per tab and
 * tells it its label, key and whether it is the open tab; clicking it switches to that tab.
 *
 * Widgets, all optional: TabButton (the clickable area), LabelText, KeyText.
 * Style the selected look with Selected/Normal Label Color, or in On Selection Changed.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UCharacterMenuTabButton : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the menu when it builds the tab bar. */
	void SetupTab(int32 InTabIndex, const FText& InLabel);

	/** The key(s) that open this tab, already formatted ("I"). Empty hides KeyText. */
	void SetKeyText(const FText& InKeyText);

	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category = "Character Menu|Tab")
	int32 GetTabIndex() const { return AssignedTabIndex; }

	UFUNCTION(BlueprintPure, Category = "Character Menu|Tab")
	bool IsSelected() const { return bIsSelected; }

	/** The player clicked this tab. The menu listens to switch tabs. */
	UPROPERTY(BlueprintAssignable, Category = "Character Menu|Tab")
	FCharacterMenuTabClickedSignature OnTabClicked;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Character Menu|Tab")
	TObjectPtr<UButton> TabButton;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Character Menu|Tab")
	TObjectPtr<UTextBlock> LabelText;

	/** Shows which key opens the tab. Hidden when the tab has no key. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Character Menu|Tab")
	TObjectPtr<UTextBlock> KeyText;

	/** Label colour while this is the open tab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Tab")
	FSlateColor SelectedLabelColor = FSlateColor(FLinearColor(0.95f, 0.78f, 0.35f, 1.0f));

	/** Label colour for the other tabs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Tab")
	FSlateColor NormalLabelColor = FSlateColor(FLinearColor(0.6f, 0.6f, 0.65f, 1.0f));

	/** Became the open tab (true) or stopped being it (false): swap a background, play an underline animation... */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Menu|Tab")
	void OnSelectionChanged(bool bNowSelected);

private:
	UFUNCTION()
	void HandleClicked();

	int32 AssignedTabIndex = INDEX_NONE;
	bool bIsSelected = false;
};
