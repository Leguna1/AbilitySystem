#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SlateWrapperTypes.h"
#include "CharacterMenuComponent.generated.h"

class APawn;
class APlayerController;
class UCharacterMenuWidget;
class UInputMappingContext;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCharacterMenuOpenedSignature, int32, OpenedTabIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCharacterMenuClosedSignature);

/** The input mode the game goes back to when the menu closes. */
UENUM(BlueprintType)
enum class ECharacterMenuGameInputMode : uint8
{
	/** Keyboard and mouse go to the game (the usual third-person setup). */
	GameOnly,

	/** The game gets input but UI can be clicked too (click-to-move, cursor always visible). */
	GameAndUI
};

/**
 * Opens and closes the full-screen Character Menu. Put it on the player controller (BP_DefaultController),
 * so the keys work while the pawn is dead or respawning.
 *
 * Setup:
 *   1. Make IMC_Menu with the tab actions (IA_Menu_Character on I, IA_Menu_Skills on K, IA_Menu_Companion on P).
 *      Set it as Menu Mapping Context; the component adds it to the player itself.
 *   2. Set Menu Widget Class (WBP_CharacterMenu). Its Tabs list says which action opens which tab.
 *
 * Opening: switches to UI-only input with the cursor, pauses the game, hides the player bar (and anything
 * else in Widgets Hidden While Open). Closing puts all of that back the way it was.
 * Inside the menu the widget reads the keys itself (game input is off): the same key again closes it.
 */
UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UCharacterMenuComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterMenuComponent();

	/** Opens the menu on TabIndex, or switches to it if the menu is already open. */
	UFUNCTION(BlueprintCallable, Category = "Character Menu")
	void OpenMenu(int32 TabIndex = 0);

	UFUNCTION(BlueprintCallable, Category = "Character Menu")
	void CloseMenu();

	/** What a tab key does: opens on that tab; closes if that tab is already open; otherwise switches to it. */
	UFUNCTION(BlueprintCallable, Category = "Character Menu")
	void ToggleTab(int32 TabIndex);

	UFUNCTION(BlueprintPure, Category = "Character Menu")
	bool IsMenuOpen() const { return bMenuOpen; }

	/** The menu widget; null until it is opened the first time. */
	UFUNCTION(BlueprintPure, Category = "Character Menu")
	UCharacterMenuWidget* GetMenuWidget() const { return MenuWidget; }

	UPROPERTY(BlueprintAssignable, Category = "Character Menu")
	FCharacterMenuOpenedSignature OnMenuOpened;

	UPROPERTY(BlueprintAssignable, Category = "Character Menu")
	FCharacterMenuClosedSignature OnMenuClosed;

	// Public to match UActorComponent, so the override doesn't narrow the base class's visibility.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	/** The menu Widget Blueprint (parent class Character Menu Widget). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu")
	TSubclassOf<UCharacterMenuWidget> MenuWidgetClass;

	/** Holds the tab actions (IMC_Menu). Added by this component, so it doesn't depend on the pawn's context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu|Input")
	TObjectPtr<UInputMappingContext> MenuMappingContext;

	/** Priority of Menu Mapping Context. Higher than the pawn's context wins if they share a key. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu|Input")
	int32 MappingPriority = 1;

	/** Input mode given back to the game on close. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu|Input")
	ECharacterMenuGameInputMode GameInputMode = ECharacterMenuGameInputMode::GameOnly;

	/** Pause the game while the menu is open. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu")
	bool bPauseGame = true;

	/** Z-order in the viewport: above the HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu")
	int32 MenuZOrder = 50;

	/**
	 * Widgets of these classes (this player's, anywhere on screen, also inside the HUD) are hidden while
	 * the menu is open and shown again on close. The player bar by default.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Menu")
	TArray<TSubclassOf<UUserWidget>> WidgetsHiddenWhileOpen;

private:
	/** A widget hidden on open, and the visibility it gets back on close. */
	struct FHiddenWidget
	{
		TWeakObjectPtr<UUserWidget> Widget;
		ESlateVisibility PreviousVisibility = ESlateVisibility::Visible;
	};

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	/** Enhanced Input callback, one binding per tab with its index. */
	void HandleTabAction(int32 TabIndex);

	/** Adds the mapping context and binds the tab actions; retries until the controller's input is ready. */
	void TrySetupInput();
	void AddMappingContext() const;
	void RemoveMappingContext() const;
	void RemoveInputBindings();

	bool EnsureMenuWidget();

	void HideWidgets();
	void RestoreWidgets();

	APlayerController* GetPlayerController() const;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMenuWidget> MenuWidget;

	TWeakObjectPtr<UInputComponent> BoundInputComponent;
	TArray<uint32> BindingHandles;
	FTimerHandle SetupRetryTimer;

	TArray<FHiddenWidget> HiddenWidgets;

	bool bMenuOpen = false;
	bool bWasPausedBeforeOpen = false;
	bool bCursorWasShownBeforeOpen = false;
};
