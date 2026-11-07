#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "CharacterMenuWidget.generated.h"

class ACharacterPreviewStudio;
class APawn;
class UButton;
class UCharacterMenuTabButton;
class UInputAction;
class UPanelWidget;
class UWidgetSwitcher;

/** Who stands in the 3D preview while a tab is open. */
UENUM(BlueprintType)
enum class ECharacterPreviewSubject : uint8
{
	/** No preview on this tab: the studio doesn't capture (Skills). */
	None,

	/** The player's pawn (Character tab). */
	Player,

	/** The player's companion, if one is out (Companion tab). */
	Companion
};

/** One tab of the Character Menu. */
USTRUCT(BlueprintType)
struct FCharacterMenuTab
{
	GENERATED_BODY()

	/** Shown on the tab button ("Character", "Skills", "Companion"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu")
	FText DisplayName;

	/**
	 * Opens the menu on this tab (IA_Menu_Character...). Map it in the menu's own mapping context (IMC_Menu).
	 * While the menu is open its keys switch to this tab, or close the menu if it's already the open tab.
	 * Empty = the tab is only reachable by clicking or with Q/E.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu")
	TObjectPtr<UInputAction> OpenAction;

	/** Who the 3D preview shows while this tab is open. None = the studio stops capturing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu")
	ECharacterPreviewSubject PreviewSubject = ECharacterPreviewSubject::None;
};

/**
 * The full-screen character menu: a tab bar, the open tab's content, a close button.
 * Opened and closed by the Character Menu Component on the player controller, which also handles
 * pausing, the cursor and hiding the player bar.
 *
 * Blueprint setup (Widget Blueprint with this as parent, e.g. WBP_CharacterMenu):
 *   - Tabs: one entry per tab, in the same order as the children of TabSwitcher.
 *   - TabSwitcher (Widget Switcher): child N is tab N's content (the existing stats, inventory, skill tree widgets...).
 *   - TabBar (any panel): one Tab Button Class widget is created per tab.
 *   - CloseButton.
 * All three are optional.
 *
 * Keys while open: game input is off (UI only), so the menu reads the keys itself. A tab's key opens
 * that tab, or closes the menu if that tab is already open. Keys are looked up through Enhanced Input,
 * so remapping a tab action also changes it here.
 *
 * Content: when the menu opens for a new pawn (first time, after a respawn), every Attribute Panel,
 * Inventory Widget and Skill Tree Widget inside it is pointed at the pawn, however deeply nested.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UCharacterMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Called by the Character Menu Component after it has shown the menu. */
	void NotifyMenuOpened(int32 TabIndex);

	/** Called by the Character Menu Component after it has hidden the menu. */
	void NotifyMenuClosed();

	/** Shows tab NewTabIndex (clamped to the tabs that exist). */
	UFUNCTION(BlueprintCallable, Category = "Character Menu")
	void SelectTab(int32 NewTabIndex);

	/** +1 = next tab, -1 = previous. Wraps around if Wrap Tabs is on. */
	UFUNCTION(BlueprintCallable, Category = "Character Menu")
	void CycleTab(int32 Direction);

	/** Closes the menu and gives the game back its input, cursor and time. */
	UFUNCTION(BlueprintCallable, Category = "Character Menu")
	void CloseMenu();

	UFUNCTION(BlueprintPure, Category = "Character Menu")
	int32 GetActiveTabIndex() const { return ActiveTabIndex; }

	UFUNCTION(BlueprintPure, Category = "Character Menu")
	int32 GetNumTabs() const { return Tabs.Num(); }

	UFUNCTION(BlueprintPure, Category = "Character Menu")
	bool IsMenuOpen() const { return bMenuOpen; }

	const TArray<FCharacterMenuTab>& GetTabs() const { return Tabs; }

	/** The 3D preview studio; null until the menu is first opened. */
	UFUNCTION(BlueprintPure, Category = "Character Menu|Preview")
	ACharacterPreviewStudio* GetPreviewStudio() const { return PreviewStudio.Get(); }

	/** Re-reads who the open tab previews (call after summoning or dismissing the companion while open). */
	UFUNCTION(BlueprintCallable, Category = "Character Menu|Preview")
	void RefreshPreview();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/* -------------------- Tabs -------------------- */

	/** The tabs, in the same order as TabSwitcher's children. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Tabs", meta = (TitleProperty = "DisplayName"))
	TArray<FCharacterMenuTab> Tabs;

	/** Created in TabBar once per tab. Empty = no tab buttons (keys still work). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu|Tabs")
	TSubclassOf<UCharacterMenuTabButton> TabButtonClass;

	/** Q on the last tab goes to the first (and E on the first to the last). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Tabs")
	bool bWrapTabs = true;

	/* -------------------- Preview -------------------- */

	/** Spawned on first open. Use a Blueprint of it to tune lights, framing and fallback animations. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu|Preview")
	TSubclassOf<ACharacterPreviewStudio> PreviewStudioClass;

	/** Where the studio is spawned: far from anything playable (it renders only its own copies, but stays out of the way). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu|Preview")
	FVector PreviewStudioLocation = FVector(0.0f, 0.0f, -50000.0f);

	/* -------------------- Keys -------------------- */

	/**
	 * Optional: the keys mapped to this action also go to the previous tab (e.g. IA_QKey, so remapping Q
	 * moves it). Only works while a mapping context with it is applied; Previous Tab Keys always work.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Keys")
	TObjectPtr<UInputAction> PreviousTabAction;

	/** Optional: like Previous Tab Action, for the next tab (e.g. IA_EKey). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Keys")
	TObjectPtr<UInputAction> NextTabAction;

	/** Always go to the previous tab, whatever the mappings say. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Keys")
	TArray<FKey> PreviousTabKeys = { EKeys::Q, EKeys::Gamepad_LeftShoulder };

	/** Always go to the next tab. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Keys")
	TArray<FKey> NextTabKeys = { EKeys::E, EKeys::Gamepad_RightShoulder };

	/** Always close the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Menu|Keys")
	TArray<FKey> CloseKeys = { EKeys::Escape, EKeys::Gamepad_Special_Right };

	/* -------------------- Bound widgets -------------------- */

	/** Tab buttons are created in here, in tab order. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Character Menu")
	TObjectPtr<UPanelWidget> TabBar;

	/** Child N is tab N's content. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Character Menu")
	TObjectPtr<UWidgetSwitcher> TabSwitcher;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Character Menu")
	TObjectPtr<UButton> CloseButton;

	/* -------------------- Events -------------------- */

	/** The menu was opened on TabIndex. Play an intro animation here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Menu")
	void OnMenuOpened(int32 TabIndex);

	/** The menu was closed (already hidden). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Menu")
	void OnMenuClosed();

	/** The open tab changed. Previous Tab Index is -1 when the menu just opened. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Menu")
	void OnTabChanged(int32 NewTabIndex, int32 PreviousTabIndex);

	/** The menu is about to show a different pawn than last time (first open, respawn). Extra content can rebind here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Menu")
	void OnContentPawnChanged(APawn* NewPawn);

private:
	UFUNCTION()
	void HandleTabButtonClicked(int32 ClickedTabIndex);

	UFUNCTION()
	void HandleCloseClicked();

	void BuildTabButtons();
	void RefreshTabButtons();

	/** Reads which keys are mapped to each tab action (and Q/E actions) right now. */
	void RefreshKeys();

	/** Points the existing stats, inventory and skill tree widgets inside the menu at Pawn. */
	void BindContentToPawn(APawn* Pawn);

	/** Index of the tab whose action Key is mapped to, or INDEX_NONE. */
	int32 FindTabForKey(const FKey& Key) const;

	/** Applies ActiveTabIndex to the switcher and buttons. */
	void ApplyActiveTab(int32 PreviousTabIndex);

	/** Spawns the studio if needed and hands it to every Character Preview Widget inside the menu. */
	void EnsurePreviewStudio();

	/** The actor the open tab previews, or null. */
	AActor* ResolvePreviewSubject() const;

	TWeakObjectPtr<ACharacterPreviewStudio> PreviewStudio;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCharacterMenuTabButton>> TabButtons;

	/** Parallel to Tabs: keys mapped to each tab's Open Action. */
	TArray<TArray<FKey>> TabKeys;

	TArray<FKey> ResolvedPreviousKeys;
	TArray<FKey> ResolvedNextKeys;

	/** The pawn the content was last bound to; rebinding happens only when it changes. */
	TWeakObjectPtr<APawn> ContentPawn;

	int32 ActiveTabIndex = INDEX_NONE;
	bool bMenuOpen = false;
};
