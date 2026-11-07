#include "CharacterMenuComponent.h"

#include "AbilitySystemLog.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CharacterMenuWidget.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerBarWidget.h"
#include "TimerManager.h"

UCharacterMenuComponent::UCharacterMenuComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	WidgetsHiddenWhileOpen.Add(UPlayerBarWidget::StaticClass());
}

/* -------------------- Lifetime -------------------- */

void UCharacterMenuComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController))
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("%s: Character Menu Component must be on a player controller."), *GetNameSafe(GetOwner()));
		return;
	}

	if (!PlayerController->IsLocalController())
	{
		return;
	}

	// Some pawns clear all mappings when possessed; put the menu's context back if so.
	PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &UCharacterMenuComponent::HandlePossessedPawnChanged);

	TrySetupInput();
}

void UCharacterMenuComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Never leave the game paused or stuck in UI input because the controller went away mid-menu.
	if (bMenuOpen)
	{
		CloseMenu();
	}

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SetupRetryTimer);
	}

	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &UCharacterMenuComponent::HandlePossessedPawnChanged);
	}

	RemoveInputBindings();
	RemoveMappingContext();

	if (IsValid(MenuWidget))
	{
		MenuWidget->RemoveFromParent();
		MenuWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

APlayerController* UCharacterMenuComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

/* -------------------- Input -------------------- */

void UCharacterMenuComponent::TrySetupInput()
{
	APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController))
	{
		return;
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerController->InputComponent);

	// The controller builds its input component when it gets its local player, which can be after BeginPlay.
	if (!IsValid(EnhancedInput) || !IsValid(PlayerController->GetLocalPlayer()))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(SetupRetryTimer, this, &UCharacterMenuComponent::TrySetupInput, 0.1f, false);
		}
		return;
	}

	AddMappingContext();
	RemoveInputBindings();

	const UCharacterMenuWidget* MenuDefaults = MenuWidgetClass ? MenuWidgetClass->GetDefaultObject<UCharacterMenuWidget>() : nullptr;

	if (!IsValid(MenuDefaults))
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("%s: Character Menu Component has no Menu Widget Class."), *GetNameSafe(GetOwner()));
		return;
	}

	// The tab list lives on the widget (next to the switcher it must match); bind each tab's action from there.
	const TArray<FCharacterMenuTab>& Tabs = MenuDefaults->GetTabs();

	for (int32 TabIndex = 0; TabIndex < Tabs.Num(); ++TabIndex)
	{
		if (IsValid(Tabs[TabIndex].OpenAction))
		{
			const FEnhancedInputActionEventBinding& Binding = EnhancedInput->BindAction(
				Tabs[TabIndex].OpenAction.Get(), ETriggerEvent::Started, this, &UCharacterMenuComponent::HandleTabAction, TabIndex);
			BindingHandles.Add(Binding.GetHandle());
		}
	}

	BoundInputComponent = EnhancedInput;
}

void UCharacterMenuComponent::AddMappingContext() const
{
	const APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController) || !IsValid(MenuMappingContext))
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		if (!InputSubsystem->HasMappingContext(MenuMappingContext))
		{
			InputSubsystem->AddMappingContext(MenuMappingContext, MappingPriority);
		}
	}
}

void UCharacterMenuComponent::RemoveMappingContext() const
{
	const APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController) || !IsValid(MenuMappingContext))
	{
		return;
	}

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		InputSubsystem->RemoveMappingContext(MenuMappingContext);
	}
}

void UCharacterMenuComponent::RemoveInputBindings()
{
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(BoundInputComponent.Get()))
	{
		for (const uint32 Handle : BindingHandles)
		{
			EnhancedInput->RemoveBindingByHandle(Handle);
		}
	}

	BindingHandles.Reset();
	BoundInputComponent.Reset();
}

void UCharacterMenuComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	AddMappingContext();
}

void UCharacterMenuComponent::HandleTabAction(const int32 TabIndex)
{
	ToggleTab(TabIndex);
}

/* -------------------- Open / close -------------------- */

void UCharacterMenuComponent::ToggleTab(const int32 TabIndex)
{
	if (!bMenuOpen)
	{
		OpenMenu(TabIndex);
	}
	else if (IsValid(MenuWidget) && MenuWidget->GetActiveTabIndex() == TabIndex)
	{
		CloseMenu();
	}
	else if (IsValid(MenuWidget))
	{
		MenuWidget->SelectTab(TabIndex);
	}
}

bool UCharacterMenuComponent::EnsureMenuWidget()
{
	if (IsValid(MenuWidget))
	{
		return true;
	}

	APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController) || !MenuWidgetClass)
	{
		return false;
	}

	MenuWidget = CreateWidget<UCharacterMenuWidget>(PlayerController, MenuWidgetClass);

	if (!IsValid(MenuWidget))
	{
		return false;
	}

	// Kept in the viewport and only collapsed between openings, so the tabs keep their state
	// (scroll position, selected item) and their bindings.
	MenuWidget->AddToViewport(MenuZOrder);
	MenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	return true;
}

void UCharacterMenuComponent::OpenMenu(const int32 TabIndex)
{
	if (bMenuOpen)
	{
		if (IsValid(MenuWidget))
		{
			MenuWidget->SelectTab(TabIndex);
		}
		return;
	}

	APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController) || !EnsureMenuWidget())
	{
		return;
	}

	bMenuOpen = true;

	// Keys held when opening (W while running) would otherwise stay "down" until pressed again.
	PlayerController->FlushPressedKeys();

	bCursorWasShownBeforeOpen = PlayerController->bShowMouseCursor;
	PlayerController->SetShowMouseCursor(true);

	MenuWidget->SetVisibility(ESlateVisibility::Visible);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);

	bWasPausedBeforeOpen = UGameplayStatics::IsGamePaused(this);

	if (bPauseGame && !bWasPausedBeforeOpen)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}

	HideWidgets();

	MenuWidget->NotifyMenuOpened(TabIndex);
	OnMenuOpened.Broadcast(MenuWidget->GetActiveTabIndex());
}

void UCharacterMenuComponent::CloseMenu()
{
	if (!bMenuOpen)
	{
		return;
	}

	bMenuOpen = false;

	if (IsValid(MenuWidget))
	{
		MenuWidget->SetVisibility(ESlateVisibility::Collapsed);
		MenuWidget->NotifyMenuClosed();
	}

	RestoreWidgets();

	if (bPauseGame && !bWasPausedBeforeOpen)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}

	if (APlayerController* PlayerController = GetPlayerController())
	{
		// The key that closed the menu (I, Esc) must not reach the game as a fresh press.
		PlayerController->FlushPressedKeys();
		PlayerController->SetShowMouseCursor(bCursorWasShownBeforeOpen);

		if (GameInputMode == ECharacterMenuGameInputMode::GameAndUI)
		{
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			PlayerController->SetInputMode(InputMode);
		}
		else
		{
			PlayerController->SetInputMode(FInputModeGameOnly());
		}
	}

	OnMenuClosed.Broadcast();
}

/* -------------------- Hidden widgets -------------------- */

void UCharacterMenuComponent::HideWidgets()
{
	HiddenWidgets.Reset();

	const APlayerController* PlayerController = GetPlayerController();

	for (const TSubclassOf<UUserWidget>& WidgetClass : WidgetsHiddenWhileOpen)
	{
		if (!WidgetClass)
		{
			continue;
		}

		TArray<UUserWidget*> Found;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Found, WidgetClass, false);

		for (UUserWidget* Widget : Found)
		{
			// Only this player's widgets, and never the menu itself.
			if (!IsValid(Widget) || Widget == MenuWidget || Widget->GetOwningPlayer() != PlayerController)
			{
				continue;
			}

			HiddenWidgets.Add({ Widget, Widget->GetVisibility() });
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UCharacterMenuComponent::RestoreWidgets()
{
	for (const FHiddenWidget& Hidden : HiddenWidgets)
	{
		if (UUserWidget* Widget = Hidden.Widget.Get())
		{
			Widget->SetVisibility(Hidden.PreviousVisibility);
		}
	}

	HiddenWidgets.Reset();
}
