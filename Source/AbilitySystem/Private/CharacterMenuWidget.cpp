#include "CharacterMenuWidget.h"

#include "AbilitySystemLog.h"
#include "AttributePanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterMenuComponent.h"
#include "CharacterMenuTabButton.h"
#include "CharacterPreviewStudio.h"
#include "CharacterPreviewWidget.h"
#include "CompanionCharacter.h"
#include "CompanionComponent.h"
#include "CompanionMenuWidget.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InventoryComponent.h"
#include "InventorySlotWidget.h"
#include "InventoryWidget.h"
#include "SkillTreeComponent.h"
#include "SkillTreeWidget.h"

namespace CharacterMenu
{
	/**
	 * Visits every widget inside Root, including the insides of nested User Widgets
	 * (the attribute panel inside WB_CharacterSheet inside the menu).
	 * Visit returns false to skip that widget's insides (a Companion Menu's panels aren't the player's).
	 */
	void ForEachWidgetDeep(const UUserWidget* Root, const TFunctionRef<bool(UWidget*)> Visit)
	{
		if (!IsValid(Root) || !IsValid(Root->WidgetTree))
		{
			return;
		}

		Root->WidgetTree->ForEachWidget([&Visit](UWidget* Widget)
		{
			if (!IsValid(Widget))
			{
				return;
			}

			const bool bDescend = Visit(Widget);

			if (const UUserWidget* Nested = Cast<UUserWidget>(Widget); Nested && bDescend)
			{
				ForEachWidgetDeep(Nested, Visit);
			}
		});
	}

	TArray<FKey> QueryKeys(const APlayerController* PlayerController, const UInputAction* Action)
	{
		if (!IsValid(PlayerController) || !IsValid(Action))
		{
			return {};
		}

		const UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());

		return IsValid(InputSubsystem) ? InputSubsystem->QueryKeysMappedToAction(Action) : TArray<FKey>();
	}

	/** "I" or "I / Tab". Gamepad keys are left out: the tab bar is read with a keyboard in mind. */
	FText FormatKeys(const TArray<FKey>& Keys)
	{
		TArray<FString> Names;

		for (const FKey& Key : Keys)
		{
			if (Key.IsValid() && !Key.IsGamepadKey())
			{
				Names.AddUnique(Key.GetDisplayName(false).ToString());
			}
		}

		return FText::FromString(FString::Join(Names, TEXT(" / ")));
	}
}

/* -------------------- Lifetime -------------------- */

void UCharacterMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Game input is off while the menu is open, so the menu itself has to take the keys.
	SetIsFocusable(true);

	if (IsValid(CloseButton))
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UCharacterMenuWidget::HandleCloseClicked);
	}

	BuildTabButtons();
}

void UCharacterMenuWidget::NativeDestruct()
{
	if (ACharacterPreviewStudio* Studio = PreviewStudio.Get())
	{
		Studio->Destroy();
	}

	PreviewStudio.Reset();
	Super::NativeDestruct();
}

void UCharacterMenuWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Clicking empty space can hand focus to the game viewport, which ignores keys in UI-only mode,
	// so Esc and the tab keys would stop working. Take focus back whenever nothing inside the menu has it.
	if (bMenuOpen && !HasAnyUserFocus() && !HasFocusedDescendants())
	{
		SetFocus();
	}
}

/* -------------------- Open / close -------------------- */

void UCharacterMenuWidget::NotifyMenuOpened(const int32 TabIndex)
{
	bMenuOpen = true;

	APawn* Pawn = GetOwningPlayerPawn();

	if (Pawn != ContentPawn.Get())
	{
		BindContentToPawn(Pawn);
	}

	// Mappings can change between openings (remapping, the pawn's context coming and going).
	RefreshKeys();

	EnsurePreviewStudio();

	// Always applied (even for the same tab as last time), with -1 as the previous tab: it's a fresh opening.
	ActiveTabIndex = Tabs.IsValidIndex(TabIndex) ? TabIndex : 0;
	ApplyActiveTab(INDEX_NONE);

	SetFocus();
	OnMenuOpened(ActiveTabIndex);
}

void UCharacterMenuWidget::NotifyMenuClosed()
{
	if (!bMenuOpen)
	{
		return;
	}

	bMenuOpen = false;

	// Hidden, not destroyed: no capturing, no lights, and no copies holding on to the subject's materials.
	if (ACharacterPreviewStudio* Studio = PreviewStudio.Get())
	{
		Studio->SetCaptureActive(false);
		Studio->SetSubject(nullptr);
	}

	OnMenuClosed();
}

void UCharacterMenuWidget::CloseMenu()
{
	const APlayerController* PlayerController = GetOwningPlayer();
	UCharacterMenuComponent* MenuComponent = IsValid(PlayerController) ? PlayerController->FindComponentByClass<UCharacterMenuComponent>() : nullptr;

	if (IsValid(MenuComponent))
	{
		MenuComponent->CloseMenu();
		return;
	}

	// Shown without a component (a test map): at least get out of the way.
	SetVisibility(ESlateVisibility::Collapsed);
	NotifyMenuClosed();
}

void UCharacterMenuWidget::HandleCloseClicked()
{
	CloseMenu();
}

/* -------------------- Tabs -------------------- */

void UCharacterMenuWidget::SelectTab(const int32 NewTabIndex)
{
	if (Tabs.IsEmpty())
	{
		return;
	}

	const int32 ClampedIndex = FMath::Clamp(NewTabIndex, 0, Tabs.Num() - 1);

	if (ClampedIndex == ActiveTabIndex)
	{
		return;
	}

	const int32 PreviousTabIndex = ActiveTabIndex;
	ActiveTabIndex = ClampedIndex;
	ApplyActiveTab(PreviousTabIndex);
}

void UCharacterMenuWidget::CycleTab(const int32 Direction)
{
	if (Tabs.IsEmpty() || Direction == 0)
	{
		return;
	}

	const int32 Step = Direction > 0 ? 1 : -1;
	int32 NewTabIndex = ActiveTabIndex + Step;

	if (bWrapTabs)
	{
		NewTabIndex = (NewTabIndex + Tabs.Num()) % Tabs.Num();
	}

	SelectTab(NewTabIndex);
}

void UCharacterMenuWidget::ApplyActiveTab(const int32 PreviousTabIndex)
{
	if (IsValid(TabSwitcher))
	{
		if (ActiveTabIndex < TabSwitcher->GetNumWidgets())
		{
			TabSwitcher->SetActiveWidgetIndex(ActiveTabIndex);
		}
		else
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("%s: tab %d has no content: TabSwitcher only has %d children."),
				*GetName(), ActiveTabIndex, TabSwitcher->GetNumWidgets());
		}
	}

	RefreshTabButtons();
	RefreshPreview();
	OnTabChanged(ActiveTabIndex, PreviousTabIndex);
}

/* -------------------- Preview -------------------- */

void UCharacterMenuWidget::EnsurePreviewStudio()
{
	if (!PreviewStudio.IsValid())
	{
		UWorld* World = GetWorld();

		if (!IsValid(World))
		{
			return;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;

		const TSubclassOf<ACharacterPreviewStudio> StudioClass = PreviewStudioClass ? PreviewStudioClass : TSubclassOf<ACharacterPreviewStudio>(ACharacterPreviewStudio::StaticClass());
		PreviewStudio = World->SpawnActor<ACharacterPreviewStudio>(StudioClass, PreviewStudioLocation, FRotator::ZeroRotator, SpawnParameters);
	}

	ACharacterPreviewStudio* Studio = PreviewStudio.Get();

	APawn* Pawn = GetOwningPlayerPawn();

	// Every open, so preview widgets added since (or nested in tab content) are covered too,
	// and companion tabs show the companion as it is now (level, a companion summoned since).
	CharacterMenu::ForEachWidgetDeep(this, [Studio, Pawn](UWidget* Widget)
	{
		if (UCharacterPreviewWidget* PreviewWidget = Cast<UCharacterPreviewWidget>(Widget))
		{
			PreviewWidget->SetStudio(Studio);
		}
		else if (UCompanionMenuWidget* CompanionMenu = Cast<UCompanionMenuWidget>(Widget))
		{
			CompanionMenu->SetLeader(Pawn);
			CompanionMenu->RefreshCompanion();
		}

		return true;
	});
}

AActor* UCharacterMenuWidget::ResolvePreviewSubject() const
{
	if (!Tabs.IsValidIndex(ActiveTabIndex))
	{
		return nullptr;
	}

	APawn* Pawn = GetOwningPlayerPawn();

	switch (Tabs[ActiveTabIndex].PreviewSubject)
	{
	case ECharacterPreviewSubject::Player:
		return Pawn;

	case ECharacterPreviewSubject::Companion:
	{
		const UCompanionComponent* Companions = IsValid(Pawn) ? Pawn->FindComponentByClass<UCompanionComponent>() : nullptr;
		return IsValid(Companions) ? Companions->GetCompanion() : nullptr;
	}

	default:
		return nullptr;
	}
}

void UCharacterMenuWidget::RefreshPreview()
{
	ACharacterPreviewStudio* Studio = PreviewStudio.Get();

	if (!bMenuOpen || !IsValid(Studio))
	{
		return;
	}

	AActor* PreviewSubject = ResolvePreviewSubject();
	Studio->SetSubject(PreviewSubject);
	Studio->SetCaptureActive(IsValid(PreviewSubject));
}

void UCharacterMenuWidget::BuildTabButtons()
{
	TabButtons.Reset();

	if (!IsValid(TabBar) || !TabButtonClass)
	{
		return;
	}

	TabBar->ClearChildren();

	for (int32 TabIndex = 0; TabIndex < Tabs.Num(); ++TabIndex)
	{
		UCharacterMenuTabButton* TabButton = CreateWidget<UCharacterMenuTabButton>(this, TabButtonClass);

		if (!IsValid(TabButton))
		{
			continue;
		}

		TabButton->SetupTab(TabIndex, Tabs[TabIndex].DisplayName);
		TabButton->OnTabClicked.AddUniqueDynamic(this, &UCharacterMenuWidget::HandleTabButtonClicked);
		TabBar->AddChild(TabButton);
		TabButtons.Add(TabButton);
	}
}

void UCharacterMenuWidget::RefreshTabButtons()
{
	for (UCharacterMenuTabButton* TabButton : TabButtons)
	{
		if (!IsValid(TabButton))
		{
			continue;
		}

		const int32 ButtonTabIndex = TabButton->GetTabIndex();
		TabButton->SetSelected(ButtonTabIndex == ActiveTabIndex);
		TabButton->SetKeyText(TabKeys.IsValidIndex(ButtonTabIndex) ? CharacterMenu::FormatKeys(TabKeys[ButtonTabIndex]) : FText::GetEmpty());
	}
}

void UCharacterMenuWidget::HandleTabButtonClicked(const int32 ClickedTabIndex)
{
	SelectTab(ClickedTabIndex);
}

/* -------------------- Keys -------------------- */

void UCharacterMenuWidget::RefreshKeys()
{
	const APlayerController* PlayerController = GetOwningPlayer();

	TabKeys.SetNum(Tabs.Num());

	for (int32 TabIndex = 0; TabIndex < Tabs.Num(); ++TabIndex)
	{
		TabKeys[TabIndex] = CharacterMenu::QueryKeys(PlayerController, Tabs[TabIndex].OpenAction);
	}

	ResolvedPreviousKeys = PreviousTabKeys;
	ResolvedPreviousKeys.Append(CharacterMenu::QueryKeys(PlayerController, PreviousTabAction));

	ResolvedNextKeys = NextTabKeys;
	ResolvedNextKeys.Append(CharacterMenu::QueryKeys(PlayerController, NextTabAction));
}

int32 UCharacterMenuWidget::FindTabForKey(const FKey& Key) const
{
	for (int32 TabIndex = 0; TabIndex < TabKeys.Num(); ++TabIndex)
	{
		if (TabKeys[TabIndex].Contains(Key))
		{
			return TabIndex;
		}
	}

	return INDEX_NONE;
}

FReply UCharacterMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bMenuOpen || InKeyEvent.IsRepeat())
	{
		return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
	}

	const FKey Key = InKeyEvent.GetKey();

	// Tab keys first, so a tab mapped to Q or E still wins over cycling.
	const int32 KeyTabIndex = FindTabForKey(Key);

	if (KeyTabIndex != INDEX_NONE)
	{
		if (KeyTabIndex == ActiveTabIndex)
		{
			CloseMenu();
		}
		else
		{
			SelectTab(KeyTabIndex);
		}

		return FReply::Handled();
	}

	if (CloseKeys.Contains(Key))
	{
		CloseMenu();
		return FReply::Handled();
	}

	if (ResolvedPreviousKeys.Contains(Key))
	{
		CycleTab(-1);
		return FReply::Handled();
	}

	if (ResolvedNextKeys.Contains(Key))
	{
		CycleTab(1);
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

/* -------------------- Content -------------------- */

void UCharacterMenuWidget::BindContentToPawn(APawn* Pawn)
{
	const APawn* OldPawn = ContentPawn.Get();
	ContentPawn = Pawn;

	USkillTreeComponent* SkillTree = IsValid(Pawn) ? Pawn->FindComponentByClass<USkillTreeComponent>() : nullptr;
	UInventoryComponent* Inventory = IsValid(Pawn) ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;

	CharacterMenu::ForEachWidgetDeep(this, [Pawn, OldPawn, SkillTree, Inventory](UWidget* Widget)
	{
		// A Companion Menu points its own parts at the companion; the player's pawn must not go in there.
		if (Widget->IsA<UCompanionMenuWidget>())
		{
			return false;
		}

		if (UAttributePanelWidget* AttributePanel = Cast<UAttributePanelWidget>(Widget))
		{
			// Leave panels showing someone else alone (a companion panel, or the sheet's companion toggle).
			if (!AttributePanel->HasCharacter() || AttributePanel->GetCharacter() == OldPawn)
			{
				AttributePanel->SetCharacter(Pawn);
			}
		}
		else if (UInventoryWidget* InventoryWidget = Cast<UInventoryWidget>(Widget))
		{
			if (InventoryWidget->GetInventory() != Inventory)
			{
				InventoryWidget->InitializeInventory(Inventory);
			}
		}
		else if (USkillTreeWidget* SkillTreeWidget = Cast<USkillTreeWidget>(Widget))
		{
			SkillTreeWidget->InitializeTree(SkillTree);
		}
		else if (UInventorySlotWidget* SlotWidget = Cast<UInventorySlotWidget>(Widget))
		{
			// Equipment slots placed outside an Inventory Widget (around the character preview).
			// Ones inside an Inventory Widget were just set up by it and already show this inventory.
			if (SlotWidget->GetEquipmentSlotTag().IsValid() && SlotWidget->GetInventory() != Inventory)
			{
				const int32 EquipmentSlotIndex = IsValid(Inventory) ? Inventory->FindEquipmentSlotIndex(SlotWidget->GetEquipmentSlotTag()) : INDEX_NONE;

				if (IsValid(Inventory) && EquipmentSlotIndex == INDEX_NONE)
				{
					UE_LOG(LogAbilitySystem, Warning, TEXT("Character Menu: slot widget %s is for %s, which the inventory has no equipment slot for."),
						*SlotWidget->GetName(), *SlotWidget->GetEquipmentSlotTag().ToString());
				}

				SlotWidget->InitializeEquipmentSlot(Inventory, EquipmentSlotIndex);
			}
		}

		return true;
	});

	OnContentPawnChanged(Pawn);
}
