#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

/** One clickable line in the Adventure Panel (dialogue, shop, blacksmith, waystone travel). */
struct FAdventurePanelOption
{
	FAdventurePanelOption() = default;

	explicit FAdventurePanelOption(const FText& InLabel, const bool bInEnabled = true)
		: Label(InLabel)
		, bEnabled(bInEnabled)
	{
	}

	FText Label;

	/** Off = shown greyed out and can't be chosen (can't afford it, level too low). */
	bool bEnabled = true;
};

/** Called with the index of the option the player chose. */
DECLARE_DELEGATE_OneParam(FAdventurePanelChoiceDelegate, int32);

/** A line in the HUD's message feed ("+120 experience", "Quest accepted"). */
struct FAdventureNotification
{
	FText Text;

	/** World time it disappears. */
	double ExpiresAt = 0.0;
};
