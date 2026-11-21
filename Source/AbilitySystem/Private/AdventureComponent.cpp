#include "AdventureComponent.h"

#include "AbilitySystemLog.h"
#include "AbilitySystemSettings.h"
#include "AdventureHUDWidget.h"
#include "AdventurePanelWidget.h"
#include "AdventureSaveGame.h"
#include "AdventureSettings.h"
#include "Animation/AnimInstance.h"
#include "AttributeComponent.h"
#include "BaseCharacter.h"
#include "CombatantComponent.h"
#include "Components/InputComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "CurrencyDataAsset.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancementComponent.h"
#include "EquipmentDataAsset.h"
#include "ExperienceComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InteractableInterface.h"
#include "InventoryComponent.h"
#include "ItemDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "LootPickup.h"
#include "QuestDataAsset.h"
#include "QuestLogComponent.h"
#include "ResourceComponent.h"
#include "SkillTreeAsset.h"
#include "SkillTreeComponent.h"
#include "TimerManager.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "Adventure"

namespace
{
	constexpr double FocusUpdateInterval = 0.1;
	constexpr float NotificationLifetime = 6.0f;
	constexpr int32 MaxNotifications = 6;

	/** Moving farther than this from where a channel started breaks it. */
	constexpr float ChannelBreakDistance = 150.0f;

	constexpr int32 HUDZOrder = 5;
	constexpr int32 PanelZOrder = 40;
}

UAdventureComponent::UAdventureComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UAdventureComponent* UAdventureComponent::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const APlayerController* PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	return IsValid(PlayerController) ? PlayerController->FindComponentByClass<UAdventureComponent>() : nullptr;
}

/* -------------------- Lifetime -------------------- */

void UAdventureComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController))
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("%s: Adventure Component must be on a player controller."), *GetNameSafe(GetOwner()));
		SetComponentTickEnabled(false);
		return;
	}

	if (!PlayerController->IsLocalController())
	{
		SetComponentTickEnabled(false);
		return;
	}

	QuestLog = PlayerController->FindComponentByClass<UQuestLogComponent>();

	if (!IsValid(QuestLog))
	{
		QuestLog = NewObject<UQuestLogComponent>(PlayerController, TEXT("QuestLog"));
		QuestLog->RegisterComponent();
	}

	HUDWidget = CreateWidget<UAdventureHUDWidget>(PlayerController, UAdventureHUDWidget::StaticClass());

	if (IsValid(HUDWidget))
	{
		HUDWidget->SetAdventure(this);
		HUDWidget->AddToViewport(HUDZOrder);
	}

	PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &UAdventureComponent::HandlePossessedPawnChanged);

	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->OnVictory.AddUniqueDynamic(this, &UAdventureComponent::HandleVictory);
	}

	NextAutosaveTime = GetWorldTime() + UAdventureSettings::Get()->AutosaveInterval;

	TrySetupInput();

	if (APawn* CurrentPawn = PlayerController->GetPawn())
	{
		SetupPawn(CurrentPawn);
	}
}

void UAdventureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Never leave the game stuck in UI input because the controller went away with the panel open.
	ClosePanel();

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InputRetryTimer);
		World->GetTimerManager().ClearTimer(PlayerSetupTimer);
	}

	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &UAdventureComponent::HandlePossessedPawnChanged);
	}

	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->OnVictory.RemoveDynamic(this, &UAdventureComponent::HandleVictory);
	}

	UnbindFromPawn();

	if (IsValid(HUDWidget))
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	if (IsValid(PanelWidget))
	{
		PanelWidget->RemoveFromParent();
		PanelWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UAdventureComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Now = GetWorldTime();

	Notifications.RemoveAll([Now](const FAdventureNotification& Notification)
	{
		return Now >= Notification.ExpiresAt;
	});

	if (Now >= NextFocusUpdateTime)
	{
		UpdateFocus();
		NextFocusUpdateTime = Now + FocusUpdateInterval;
	}

	UpdateChannel();

	if (bRespawning && Now >= RespawnAtTime)
	{
		Respawn();
	}

	const float AutosaveInterval = UAdventureSettings::Get()->AutosaveInterval;

	if (AutosaveInterval > 0.0f && Now >= NextAutosaveTime)
	{
		NextAutosaveTime = Now + AutosaveInterval;

		// Not while dead: the save would put the player back on the spot that killed them.
		if (bPlayerReady && !bRespawning)
		{
			RequestSave();
		}
	}

	if (IsValid(HUDWidget))
	{
		HUDWidget->RefreshHUD();
	}
}

double UAdventureComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

/* -------------------- Who -------------------- */

APlayerController* UAdventureComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetOwner());
}

APawn* UAdventureComponent::GetPlayerPawn() const
{
	return BoundPawn.Get();
}

UInventoryComponent* UAdventureComponent::GetInventory() const
{
	const APawn* PlayerPawn = BoundPawn.Get();
	return IsValid(PlayerPawn) ? PlayerPawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

UEnhancementComponent* UAdventureComponent::GetEnhancement() const
{
	const APawn* PlayerPawn = BoundPawn.Get();
	return IsValid(PlayerPawn) ? PlayerPawn->FindComponentByClass<UEnhancementComponent>() : nullptr;
}

/* -------------------- Input -------------------- */

void UAdventureComponent::TrySetupInput()
{
	if (bInputBound)
	{
		return;
	}

	APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController))
	{
		return;
	}

	UInputComponent* Input = PlayerController->InputComponent;

	// The controller builds its input component when it gets its local player, which can be after BeginPlay.
	if (!IsValid(Input))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(InputRetryTimer, this, &UAdventureComponent::TrySetupInput, 0.1f, false);
		}

		return;
	}

	const UAdventureSettings* Settings = UAdventureSettings::Get();

	// Plain key bindings: no Input Actions or Mapping Contexts to create. They don't consume the key,
	// so an Enhanced Input action on the same key still fires.
	if (Settings->InteractKey.IsValid())
	{
		FInputKeyBinding& InteractBinding = Input->BindKey(Settings->InteractKey, IE_Pressed, this, &UAdventureComponent::HandleInteractPressed);
		InteractBinding.bConsumeInput = false;
	}

	if (Settings->QuestLogKey.IsValid())
	{
		FInputKeyBinding& QuestLogBinding = Input->BindKey(Settings->QuestLogKey, IE_Pressed, this, &UAdventureComponent::HandleQuestLogPressed);
		QuestLogBinding.bConsumeInput = false;
	}

	bInputBound = true;
}

void UAdventureComponent::HandleInteractPressed()
{
	TryInteract();
}

void UAdventureComponent::HandleQuestLogPressed()
{
	ToggleQuestLog();
}

/* -------------------- Player pawn -------------------- */

void UAdventureComponent::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	SetupPawn(NewPawn);
}

void UAdventureComponent::UnbindFromPawn()
{
	if (IsValid(BoundCombatant))
	{
		BoundCombatant->OnDied.RemoveDynamic(this, &UAdventureComponent::HandlePlayerDied);
		BoundCombatant->OnPayloadReceived.RemoveDynamic(this, &UAdventureComponent::HandlePlayerDamaged);
	}

	BoundCombatant = nullptr;
	BoundPawn.Reset();
}

void UAdventureComponent::SetupPawn(APawn* NewPawn)
{
	if (NewPawn == BoundPawn.Get())
	{
		return;
	}

	UnbindFromPawn();
	bPlayerReady = false;

	if (IsValid(QuestLog))
	{
		QuestLog->SetPlayerPawn(NewPawn);
	}

	if (!IsValid(NewPawn))
	{
		return;
	}

	BoundPawn = NewPawn;
	BoundCombatant = NewPawn->FindComponentByClass<UCombatantComponent>();

	if (IsValid(BoundCombatant))
	{
		BoundCombatant->OnDied.AddUniqueDynamic(this, &UAdventureComponent::HandlePlayerDied);
		BoundCombatant->OnPayloadReceived.AddUniqueDynamic(this, &UAdventureComponent::HandlePlayerDamaged);
	}
	else
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("%s: the player pawn %s has no Combatant Component, so death and respawn are off."),
			*GetNameSafe(GetOwner()), *GetNameSafe(NewPawn));
	}

	// Where the player wakes up until a waystone is attuned.
	if (!bHasStartTransform)
	{
		StartTransform = NewPawn->GetActorTransform();
		bHasStartTransform = true;
	}

	if (!IsValid(NewPawn->FindComponentByClass<UEnhancementComponent>()))
	{
		UEnhancementComponent* Enhancement = NewObject<UEnhancementComponent>(NewPawn, TEXT("EnhancementComponent"));
		Enhancement->RegisterComponent();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(PlayerSetupTimer, this, &UAdventureComponent::FinishPlayerSetup, 0.2f, false);
	}
}

void UAdventureComponent::FinishPlayerSetup()
{
	APawn* PlayerPawn = BoundPawn.Get();

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this);

	if (!bSaveApplied)
	{
		bSaveApplied = true;

		const UAdventureSaveGame* Save = IsValid(Progress) ? Progress->GetLoadedSave() : nullptr;

		if (IsValid(Save) && Save->bHasPlayerData)
		{
			ApplySaveData(*Save);
		}
	}

	bPlayerReady = true;

	if (IsValid(Progress))
	{
		Progress->NotifyPlayerReady(PlayerPawn);
	}
}

/* -------------------- Messages -------------------- */

void UAdventureComponent::Notify(const FText& Message)
{
	if (Message.IsEmpty())
	{
		return;
	}

	FAdventureNotification& Notification = Notifications.AddDefaulted_GetRef();
	Notification.Text = Message;
	Notification.ExpiresAt = GetWorldTime() + NotificationLifetime;

	while (Notifications.Num() > MaxNotifications)
	{
		Notifications.RemoveAt(0);
	}
}

void UAdventureComponent::ShowBanner(const FText& Title, const FText& Subtitle, const float Duration)
{
	BannerTitle = Title;
	BannerSubtitle = Subtitle;
	BannerExpiresAt = GetWorldTime() + FMath::Max(Duration, 0.5f);
}

bool UAdventureComponent::GetBanner(FText& OutTitle, FText& OutSubtitle) const
{
	if (GetWorldTime() >= BannerExpiresAt)
	{
		return false;
	}

	OutTitle = BannerTitle;
	OutSubtitle = BannerSubtitle;
	return true;
}

void UAdventureComponent::HandleVictory()
{
	ShowBanner(LOCTEXT("VictoryTitle", "Light returns to the world"),
		LOCTEXT("VictorySubtitle", "Every altar is cleansed and every guardian has fallen."), 12.0f);
}

/* -------------------- Rewards -------------------- */

UCurrencyDataAsset* UAdventureComponent::GetGoldCurrency() const
{
	return UAdventureSettings::Get()->GoldCurrency.LoadSynchronous();
}

void UAdventureComponent::GiveExperience(const int32 Amount)
{
	const APawn* PlayerPawn = BoundPawn.Get();
	UExperienceComponent* Experience = IsValid(PlayerPawn) ? PlayerPawn->FindComponentByClass<UExperienceComponent>() : nullptr;

	if (Amount <= 0 || !IsValid(Experience))
	{
		return;
	}

	Experience->AddExperience(Amount);
	Notify(FText::Format(LOCTEXT("GainedExperience", "+{0} experience"), Amount));
}

void UAdventureComponent::GiveGold(const int32 Amount)
{
	UInventoryComponent* Inventory = GetInventory();
	UCurrencyDataAsset* Gold = GetGoldCurrency();

	if (Amount <= 0 || !IsValid(Inventory) || !IsValid(Gold))
	{
		return;
	}

	Inventory->AddCurrency(Gold, Amount);
	Notify(FText::Format(LOCTEXT("GainedGold", "+{0} {1}"), Amount, Gold->GetDisplayName()));
}

void UAdventureComponent::GiveItem(UItemDataAsset* Item, const int32 Count)
{
	APawn* PlayerPawn = BoundPawn.Get();
	UInventoryComponent* Inventory = GetInventory();

	if (!IsValid(Item) || Count <= 0 || !IsValid(PlayerPawn) || !IsValid(Inventory))
	{
		return;
	}

	const int32 Leftover = Inventory->AddItem(Item, Count);

	Notify(Count > 1
		? FText::Format(LOCTEXT("ReceivedItems", "Received: {0} x{1}"), Item->GetDisplayName(), Count)
		: FText::Format(LOCTEXT("ReceivedItem", "Received: {0}"), Item->GetDisplayName()));

	if (Leftover <= 0)
	{
		return;
	}

	// The bag is full: the rest waits on the ground instead of being lost.
	TSubclassOf<ALootPickup> PickupClass = GetDefault<UAbilitySystemSettings>()->DefaultLootPickupClass.LoadSynchronous();

	if (!PickupClass)
	{
		PickupClass = ALootPickup::StaticClass();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector DropLocation = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector() * 120.0f;

	if (ALootPickup* Pickup = GetWorld()->SpawnActor<ALootPickup>(PickupClass, DropLocation, FRotator::ZeroRotator, Params))
	{
		Pickup->InitializePickup(Item, Leftover);
		Notify(LOCTEXT("BagFull", "Your bag is full. The rest is on the ground."));
	}
}

int32 UAdventureComponent::GetGold() const
{
	const UInventoryComponent* Inventory = GetInventory();
	const UCurrencyDataAsset* Gold = GetGoldCurrency();
	return IsValid(Inventory) && IsValid(Gold) ? Inventory->GetCurrency(Gold) : 0;
}

bool UAdventureComponent::SpendGold(const int32 Amount)
{
	UInventoryComponent* Inventory = GetInventory();
	UCurrencyDataAsset* Gold = GetGoldCurrency();

	if (Amount <= 0 || !IsValid(Gold))
	{
		return true;
	}

	return IsValid(Inventory) && Inventory->SpendCurrency(Gold, Amount);
}

/* -------------------- Boss bar -------------------- */

void UAdventureComponent::SetActiveBoss(ABaseCharacter* Boss)
{
	ActiveBoss = Boss;
}

/* -------------------- Channel -------------------- */

bool UAdventureComponent::BeginChannel(const FText& Label, const float Duration, const FSimpleDelegate& OnCompleted)
{
	const APawn* PlayerPawn = BoundPawn.Get();

	if (bChanneling || bRespawning || !IsValid(PlayerPawn))
	{
		return false;
	}

	bChanneling = true;
	ChannelLabel = Label;
	ChannelStartTime = GetWorldTime();
	ChannelDuration = FMath::Max(Duration, 0.1f);
	ChannelStartLocation = PlayerPawn->GetActorLocation();
	ChannelCompleted = OnCompleted;
	return true;
}

void UAdventureComponent::CancelChannel()
{
	bChanneling = false;
	ChannelCompleted.Unbind();
}

float UAdventureComponent::GetChannelProgress() const
{
	if (!bChanneling || ChannelDuration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(GetWorldTime() - ChannelStartTime) / ChannelDuration, 0.0f, 1.0f);
}

void UAdventureComponent::UpdateChannel()
{
	if (!bChanneling)
	{
		return;
	}

	const APawn* PlayerPawn = BoundPawn.Get();

	if (!IsValid(PlayerPawn) || FVector::Dist(PlayerPawn->GetActorLocation(), ChannelStartLocation) > ChannelBreakDistance)
	{
		CancelChannel();
		Notify(LOCTEXT("ChannelMoved", "Interrupted: you moved away."));
		return;
	}

	if (GetChannelProgress() >= 1.0f)
	{
		// Copied first: the callback may start another channel.
		const FSimpleDelegate Completed = ChannelCompleted;
		CancelChannel();
		Completed.ExecuteIfBound();
	}
}

void UAdventureComponent::HandlePlayerDamaged(const FAbilityPayload& Payload, const float DamageApplied)
{
	if (bChanneling && DamageApplied > 0.0f)
	{
		CancelChannel();
		Notify(LOCTEXT("ChannelHit", "Interrupted: you were hit."));
	}
}

/* -------------------- Panel -------------------- */

void UAdventureComponent::OpenPanel(const FText& Title, const FText& Body, const TArray<FAdventurePanelOption>& Options, const FAdventurePanelChoiceDelegate& OnChosen)
{
	APlayerController* PlayerController = GetPlayerController();

	if (!IsValid(PlayerController) || bRespawning)
	{
		return;
	}

	if (!IsValid(PanelWidget))
	{
		PanelWidget = CreateWidget<UAdventurePanelWidget>(PlayerController, UAdventurePanelWidget::StaticClass());

		if (!IsValid(PanelWidget))
		{
			return;
		}

		PanelWidget->OnOptionChosen.BindUObject(this, &UAdventureComponent::HandlePanelOption);
		PanelWidget->OnCloseRequested.BindUObject(this, &UAdventureComponent::ClosePanel);
		PanelWidget->AddToViewport(PanelZOrder);
		PanelWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	PanelChoice = OnChosen;
	bQuestLogShown = false;
	PanelWidget->ShowPanel(Title, Body, Options);

	if (!bPanelOpen)
	{
		bPanelOpen = true;
		CancelChannel();

		// Keys held when opening (W while running) would otherwise stay "down" until pressed again.
		PlayerController->FlushPressedKeys();

		bCursorWasShownBeforePanel = PlayerController->bShowMouseCursor;
		PlayerController->SetShowMouseCursor(true);
		PanelWidget->SetVisibility(ESlateVisibility::Visible);

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(PanelWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}

	// Rebuilding the options removes the button that had focus; the panel takes the keys back.
	PanelWidget->FocusPanel();
}

void UAdventureComponent::ClosePanel()
{
	if (!bPanelOpen)
	{
		return;
	}

	bPanelOpen = false;
	bQuestLogShown = false;
	PanelChoice.Unbind();

	if (IsValid(PanelWidget))
	{
		PanelWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (APlayerController* PlayerController = GetPlayerController())
	{
		// The key that closed the panel must not reach the game as a fresh press.
		PlayerController->FlushPressedKeys();
		PlayerController->SetShowMouseCursor(bCursorWasShownBeforePanel);

		if (bCursorWasShownBeforePanel)
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
}

void UAdventureComponent::HandlePanelOption(const int32 OptionIndex)
{
	// Copied first: the handler usually opens the next page, which replaces Panel Choice.
	const FAdventurePanelChoiceDelegate Handler = PanelChoice;
	Handler.ExecuteIfBound(OptionIndex);
}

void UAdventureComponent::ToggleQuestLog()
{
	if (bPanelOpen)
	{
		if (bQuestLogShown)
		{
			ClosePanel();
		}

		return;
	}

	ShowQuestLogPage();
}

void UAdventureComponent::ShowQuestLogPage()
{
	FString Body;

	const FText Tracker = IsValid(QuestLog) ? QuestLog->BuildTrackerText() : FText::GetEmpty();
	Body += Tracker.IsEmpty() ? FString(TEXT("No active quests. Villagers marked with ! have work for you.")) : Tracker.ToString();

	if (const UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		const TArray<FWorldObjective>& Objectives = Progress->GetObjectives();

		if (!Objectives.IsEmpty())
		{
			Body += FString::Printf(TEXT("\n\nThe world (%d/%d)"), Progress->GetCompletedObjectiveCount(), Objectives.Num());

			for (const FWorldObjective& Objective : Objectives)
			{
				Body += FString::Printf(TEXT("\n  [%s] %s"),
					Progress->HasFlag(Objective.Flag) ? TEXT("x") : TEXT("  "),
					*Objective.DisplayName.ToString());
			}
		}
	}

	TArray<FAdventurePanelOption> Options;
	Options.Add(FAdventurePanelOption(LOCTEXT("Close", "Close")));

	OpenPanel(LOCTEXT("QuestLogTitle", "Quest Log"), FText::FromString(Body), Options,
		FAdventurePanelChoiceDelegate::CreateUObject(this, &UAdventureComponent::HandleQuestLogOption));

	bQuestLogShown = bPanelOpen;
}

void UAdventureComponent::HandleQuestLogOption(const int32 OptionIndex)
{
	ClosePanel();
}

/* -------------------- Interaction -------------------- */

void UAdventureComponent::UpdateFocus()
{
	FocusedInteractable.Reset();
	FocusPrompt = FText::GetEmpty();
	bFocusCanInteract = false;

	APawn* PlayerPawn = BoundPawn.Get();
	const UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this);

	if (!IsValid(PlayerPawn) || !IsValid(Progress) || bRespawning || bPanelOpen)
	{
		return;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	const double Radius = UAdventureSettings::Get()->InteractionRadius;
	double BestDistanceSquared = Radius * Radius;
	AActor* Best = nullptr;
	FText BestPrompt;

	for (const TWeakObjectPtr<AActor>& Candidate : Progress->GetInteractables())
	{
		AActor* Actor = Candidate.Get();

		if (!IsValid(Actor) || Actor == PlayerPawn)
		{
			continue;
		}

		// Distance to the actor's collision bounds, so a wide gate can be used from anywhere along it.
		FVector BoundsOrigin;
		FVector BoundsExtent;
		Actor->GetActorBounds(true, BoundsOrigin, BoundsExtent);

		const double DistanceSquared = BoundsExtent.IsNearlyZero()
			? FVector::DistSquared(Actor->GetActorLocation(), PlayerLocation)
			: FBox(BoundsOrigin - BoundsExtent, BoundsOrigin + BoundsExtent).ComputeSquaredDistanceToPoint(PlayerLocation);

		if (DistanceSquared > BestDistanceSquared)
		{
			continue;
		}

		const FText Prompt = IInteractableInterface::Execute_GetInteractionPrompt(Actor, PlayerPawn);

		if (Prompt.IsEmpty())
		{
			continue;
		}

		Best = Actor;
		BestPrompt = Prompt;
		BestDistanceSquared = DistanceSquared;
	}

	if (IsValid(Best))
	{
		FocusedInteractable = Best;
		FocusPrompt = BestPrompt;
		bFocusCanInteract = IInteractableInterface::Execute_CanInteract(Best, PlayerPawn);
	}
}

void UAdventureComponent::TryInteract()
{
	if (bPanelOpen || bRespawning || bChanneling)
	{
		return;
	}

	UpdateFocus();

	AActor* Target = FocusedInteractable.Get();
	APawn* PlayerPawn = BoundPawn.Get();

	if (IsValid(Target) && IsValid(PlayerPawn) && bFocusCanInteract)
	{
		IInteractableInterface::Execute_Interact(Target, PlayerPawn);

		// The prompt usually changes after use (an opened chest has none).
		NextFocusUpdateTime = 0.0;
	}
}

/* -------------------- Death and travel -------------------- */

void UAdventureComponent::SetCheckpoint(const FTransform& NewCheckpoint)
{
	Checkpoint = NewCheckpoint;
	bHasCheckpoint = true;
}

float UAdventureComponent::GetRespawnTimeRemaining() const
{
	return bRespawning ? static_cast<float>(FMath::Max(RespawnAtTime - GetWorldTime(), 0.0)) : 0.0f;
}

void UAdventureComponent::HandlePlayerDied(AActor* Killer)
{
	if (bRespawning)
	{
		return;
	}

	CancelChannel();
	ClosePanel();

	bRespawning = true;
	RespawnAtTime = GetWorldTime() + UAdventureSettings::Get()->RespawnDelay;
}

void UAdventureComponent::TeleportPlayer(const FTransform& Destination)
{
	APawn* PlayerPawn = BoundPawn.Get();

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	const FRotator Facing(0.0f, Destination.Rotator().Yaw, 0.0f);

	if (const ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
	{
		if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}

	PlayerPawn->TeleportTo(Destination.GetLocation(), Facing, false, true);

	if (APlayerController* PlayerController = GetPlayerController())
	{
		PlayerController->SetControlRotation(Facing);
	}
}

void UAdventureComponent::RestorePlayer()
{
	const APawn* PlayerPawn = BoundPawn.Get();

	if (UResourceComponent* Resources = IsValid(PlayerPawn) ? PlayerPawn->FindComponentByClass<UResourceComponent>() : nullptr)
	{
		Resources->RefillResources();
	}
}

void UAdventureComponent::Respawn()
{
	bRespawning = false;

	APawn* PlayerPawn = BoundPawn.Get();

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	const UAdventureSettings* Settings = UAdventureSettings::Get();

	// The price of dying, taken before waking up so the message makes sense.
	const int32 GoldLost = FMath::FloorToInt(static_cast<float>(GetGold()) * Settings->DeathGoldLossFraction);

	if (GoldLost > 0 && SpendGold(GoldLost))
	{
		Notify(FText::Format(LOCTEXT("GoldLost", "You lost {0} gold."), GoldLost));
	}

	TeleportPlayer(bHasCheckpoint ? Checkpoint : StartTransform);

	if (UResourceComponent* Resources = PlayerPawn->FindComponentByClass<UResourceComponent>())
	{
		Resources->Revive(Settings->RespawnHealthFraction);

		const float MaxFocus = Resources->GetResourceValue(EResourceType::Focus, EResourceValueType::Max);
		Resources->ModifyResource(EResourceType::Focus, EResourceValueType::Current, MaxFocus);
		Resources->RestartFocusRegeneration();
	}

	if (const ACharacter* PlayerCharacter = Cast<ACharacter>(PlayerPawn))
	{
		// The death montage holds its last frame; stopping it lets the character stand up again.
		const USkeletalMeshComponent* CharacterMesh = PlayerCharacter->GetMesh();

		if (UAnimInstance* AnimInstance = IsValid(CharacterMesh) ? CharacterMesh->GetAnimInstance() : nullptr)
		{
			AnimInstance->StopAllMontages(0.2f);
		}

		if (UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	Notify(bHasCheckpoint
		? LOCTEXT("WokeAtWaystone", "You wake at the waystone.")
		: LOCTEXT("WokeAtStart", "You wake where your journey began."));
}

/* -------------------- Saving -------------------- */

void UAdventureComponent::RequestSave()
{
	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->SaveGame();
	}
}

void UAdventureComponent::WriteSaveData(UAdventureSaveGame& Save) const
{
	const APawn* PlayerPawn = BoundPawn.Get();

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	Save.bHasPlayerData = true;
	Save.bHasCheckpoint = bHasCheckpoint;
	Save.Checkpoint = Checkpoint;
	Save.CheckpointLevel = UGameplayStatics::GetCurrentLevelName(this, true);

	if (const UExperienceComponent* Experience = PlayerPawn->FindComponentByClass<UExperienceComponent>())
	{
		Save.Level = Experience->GetLevel();
		Save.Experience = Experience->GetExperience();
	}

	Save.CoreStatPoints.Reset();

	if (const UAttributeComponent* Attributes = PlayerPawn->FindComponentByClass<UAttributeComponent>())
	{
		for (const FCoreStatDefinition& CoreStat : Attributes->GetCoreStats())
		{
			const int32 Spent = Attributes->GetSpentPoints(CoreStat.CoreStat);

			if (Spent > 0)
			{
				FSavedTagValue& Saved = Save.CoreStatPoints.AddDefaulted_GetRef();
				Saved.Tag = CoreStat.CoreStat;
				Saved.Value = Spent;
			}
		}
	}

	Save.SkillNodeRanks.Reset();

	if (const USkillTreeComponent* SkillTree = PlayerPawn->FindComponentByClass<USkillTreeComponent>())
	{
		if (const USkillTreeAsset* Tree = SkillTree->GetTreeAsset())
		{
			for (const FSkillTreeNode& Node : Tree->Nodes)
			{
				const int32 Rank = SkillTree->GetNodeLearnedRank(Node.NodeId);

				if (Rank > 0)
				{
					FSavedTagValue& Saved = Save.SkillNodeRanks.AddDefaulted_GetRef();
					Saved.Tag = Node.NodeId;
					Saved.Value = Rank;
				}
			}
		}
	}

	Save.BagItems.Reset();
	Save.EquippedItems.Reset();
	Save.Gold = GetGold();

	if (const UInventoryComponent* Inventory = PlayerPawn->FindComponentByClass<UInventoryComponent>())
	{
		for (int32 BagIndex = 0; BagIndex < Inventory->GetNumSlots(); ++BagIndex)
		{
			const FInventorySlot Stack = Inventory->GetSlot(BagIndex);

			if (!Stack.IsEmpty())
			{
				FSavedItemStack& Saved = Save.BagItems.AddDefaulted_GetRef();
				Saved.Item = FSoftObjectPath(Stack.Item.Get());
				Saved.Count = Stack.Count;
			}
		}

		for (int32 EquipmentIndex = 0; EquipmentIndex < Inventory->GetNumEquipmentSlots(); ++EquipmentIndex)
		{
			if (const UEquipmentDataAsset* Worn = Inventory->GetEquippedItem(EquipmentIndex))
			{
				Save.EquippedItems.Add(FSoftObjectPath(Worn));
			}
		}
	}

	if (const UEnhancementComponent* Enhancement = PlayerPawn->FindComponentByClass<UEnhancementComponent>())
	{
		Enhancement->WriteSaveData(Save);
	}

	if (IsValid(QuestLog))
	{
		QuestLog->WriteSaveData(Save);
	}
}

void UAdventureComponent::ApplySaveData(const UAdventureSaveGame& Save)
{
	APawn* PlayerPawn = BoundPawn.Get();

	if (!IsValid(PlayerPawn))
	{
		return;
	}

	// Level first: it hands out the skill and attribute points the next steps spend again.
	if (UExperienceComponent* Experience = PlayerPawn->FindComponentByClass<UExperienceComponent>())
	{
		Experience->RestoreProgress(Save.Level, Save.Experience);
	}

	if (UAttributeComponent* Attributes = PlayerPawn->FindComponentByClass<UAttributeComponent>())
	{
		for (const FSavedTagValue& Saved : Save.CoreStatPoints)
		{
			// One at a time: if the stat's cap or the point total changed since the save, as many as still fit go in.
			for (int32 Point = Attributes->GetSpentPoints(Saved.Tag); Point < Saved.Value; ++Point)
			{
				if (!Attributes->SpendAttributePoints(Saved.Tag, 1))
				{
					break;
				}
			}
		}
	}

	if (USkillTreeComponent* SkillTree = PlayerPawn->FindComponentByClass<USkillTreeComponent>())
	{
		// Nodes unlock in prerequisite order, which the save doesn't keep: sweep until a pass changes nothing.
		bool bProgress = true;

		for (int32 Pass = 0; bProgress && Pass < 32; ++Pass)
		{
			bProgress = false;

			for (const FSavedTagValue& Saved : Save.SkillNodeRanks)
			{
				while (SkillTree->GetNodeLearnedRank(Saved.Tag) < Saved.Value && SkillTree->UnlockNode(Saved.Tag))
				{
					bProgress = true;
				}
			}
		}
	}

	if (UInventoryComponent* Inventory = PlayerPawn->FindComponentByClass<UInventoryComponent>())
	{
		// The pawn starts every session with its Starting Items and Starting Equipment; the save replaces them.
		const auto EmptyBag = [Inventory]()
		{
			for (int32 BagIndex = 0; BagIndex < Inventory->GetNumSlots(); ++BagIndex)
			{
				const int32 Held = Inventory->GetSlot(BagIndex).Count;

				if (!Inventory->IsSlotEmpty(BagIndex) && Held > 0)
				{
					Inventory->RemoveItemAt(BagIndex, Held);
				}
			}
		};

		EmptyBag();

		for (int32 EquipmentIndex = 0; EquipmentIndex < Inventory->GetNumEquipmentSlots(); ++EquipmentIndex)
		{
			if (IsValid(Inventory->GetEquippedItem(EquipmentIndex)))
			{
				Inventory->Unequip(EquipmentIndex);
			}
		}

		EmptyBag();

		for (const FSoftObjectPath& SavedEquipment : Save.EquippedItems)
		{
			UItemDataAsset* Item = Cast<UItemDataAsset>(SavedEquipment.TryLoad());

			if (!IsValid(Item) || Inventory->AddItem(Item, 1) > 0)
			{
				continue;
			}

			for (int32 BagIndex = 0; BagIndex < Inventory->GetNumSlots(); ++BagIndex)
			{
				if (Inventory->GetSlot(BagIndex).Item == Item)
				{
					// If the slot is already taken (two items saved for one slot), the item simply stays in the bag.
					Inventory->EquipFromSlot(BagIndex);
					break;
				}
			}
		}

		for (const FSavedItemStack& Saved : Save.BagItems)
		{
			if (UItemDataAsset* Item = Cast<UItemDataAsset>(Saved.Item.TryLoad()))
			{
				Inventory->AddItem(Item, Saved.Count);
			}
		}

		if (UCurrencyDataAsset* Gold = GetGoldCurrency())
		{
			const int32 Held = Inventory->GetCurrency(Gold);

			if (Held > Save.Gold)
			{
				Inventory->SpendCurrency(Gold, Held - Save.Gold);
			}
			else if (Held < Save.Gold)
			{
				Inventory->AddCurrency(Gold, Save.Gold - Held);
			}
		}
	}

	if (UEnhancementComponent* Enhancement = PlayerPawn->FindComponentByClass<UEnhancementComponent>())
	{
		Enhancement->ApplySaveData(Save);
	}

	if (IsValid(QuestLog))
	{
		QuestLog->ApplySaveData(Save);
	}

	if (Save.bHasCheckpoint && Save.CheckpointLevel == UGameplayStatics::GetCurrentLevelName(this, true))
	{
		SetCheckpoint(Save.Checkpoint);
		TeleportPlayer(Save.Checkpoint);
	}
}

#undef LOCTEXT_NAMESPACE
