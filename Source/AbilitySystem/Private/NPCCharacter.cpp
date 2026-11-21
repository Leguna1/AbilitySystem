#include "NPCCharacter.h"

#include "AdventureComponent.h"
#include "AdventureSettings.h"
#include "AdventureTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "CurrencyDataAsset.h"
#include "Engine/StaticMesh.h"
#include "EnhancementComponent.h"
#include "EquipmentDataAsset.h"
#include "InventoryComponent.h"
#include "ItemDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "QuestDataAsset.h"
#include "QuestLogComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "NPCCharacter"

ANPCCharacter::ANPCCharacter()
{
	// Only to turn the name toward the camera.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	// Villagers stand where they are placed; nothing possesses them.
	AutoPossessAI = EAutoPossessAI::Disabled;

	Placeholder = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Placeholder"));
	Placeholder->SetupAttachment(GetCapsuleComponent());
	Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Placeholder->SetGenerateOverlapEvents(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PillarMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	if (PillarMesh.Succeeded())
	{
		Placeholder->SetStaticMesh(PillarMesh.Object);

		// About the size of a person inside the default capsule.
		Placeholder->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.76f));
	}

	NameLabel = CreateDefaultSubobject<UTextRenderComponent>(TEXT("NameLabel"));
	NameLabel->SetupAttachment(GetCapsuleComponent());
	NameLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	NameLabel->SetHorizontalAlignment(EHTA_Center);
	NameLabel->SetVerticalAlignment(EVRTA_TextBottom);
	NameLabel->SetWorldSize(24.0f);
	NameLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DisplayName = LOCTEXT("DefaultName", "Villager");
	Greeting = LOCTEXT("DefaultGreeting", "Well met, traveler.");
}

void ANPCCharacter::BeginPlay()
{
	Super::BeginPlay();

	const USkeletalMeshComponent* CharacterMesh = GetMesh();

	if (IsValid(CharacterMesh) && IsValid(CharacterMesh->GetSkeletalMeshAsset()))
	{
		Placeholder->SetVisibility(false);
	}

	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->RegisterInteractable(this);
	}

	// Quests become available as the player levels and the world changes; a slow look is enough for a marker.
	GetWorldTimerManager().SetTimer(MarkerTimer, this, &ANPCCharacter::RefreshMarker, 1.0f, true);
	RefreshMarker();
}

void ANPCCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(MarkerTimer);

	if (UQuestLogComponent* QuestLog = BoundQuestLog.Get())
	{
		QuestLog->OnQuestLogChanged.RemoveDynamic(this, &ANPCCharacter::HandleQuestLogChanged);
	}

	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->UnregisterInteractable(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ANPCCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Text Render doesn't face the camera by itself.
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = Camera->GetCameraLocation() - NameLabel->GetComponentLocation();
		NameLabel->SetWorldRotation(FRotator(0.0f, ToCamera.Rotation().Yaw, 0.0f));
	}
}

UAdventureComponent* ANPCCharacter::GetAdventure() const
{
	return UAdventureComponent::Get(this);
}

UQuestLogComponent* ANPCCharacter::GetQuestLog() const
{
	const UAdventureComponent* Adventure = GetAdventure();
	return IsValid(Adventure) ? Adventure->GetQuestLog() : nullptr;
}

/* -------------------- Marker -------------------- */

void ANPCCharacter::HandleQuestLogChanged()
{
	RefreshMarker();
}

void ANPCCharacter::RefreshMarker()
{
	UQuestLogComponent* QuestLog = GetQuestLog();

	// The player's quest log may not exist yet when the villager begins play; hook up once it does.
	if (IsValid(QuestLog) && BoundQuestLog.Get() != QuestLog)
	{
		QuestLog->OnQuestLogChanged.AddUniqueDynamic(this, &ANPCCharacter::HandleQuestLogChanged);
		BoundQuestLog = QuestLog;
	}

	bool bHasQuestToGive = false;
	bool bHasQuestToTakeBack = false;

	if (IsValid(QuestLog))
	{
		for (const UQuestDataAsset* Quest : Quests)
		{
			const EQuestState State = QuestLog->GetQuestState(Quest);
			bHasQuestToGive |= State == EQuestState::Available;
			bHasQuestToTakeBack |= State == EQuestState::ReadyToTurnIn;
		}
	}

	FString Text;

	if (bHasQuestToTakeBack)
	{
		Text = TEXT("?<br>");
	}
	else if (bHasQuestToGive)
	{
		Text = TEXT("!<br>");
	}

	Text += DisplayName.ToString();

	if (!Title.IsEmpty())
	{
		Text += FString::Printf(TEXT("<br>- %s -"), *Title.ToString());
	}

	NameLabel->SetText(FText::FromString(Text));
	NameLabel->SetTextRenderColor(bHasQuestToGive || bHasQuestToTakeBack ? FColor(255, 210, 60) : FColor::White);
}

/* -------------------- Talking -------------------- */

FText ANPCCharacter::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return FText::Format(LOCTEXT("Talk", "Talk to {0}"), DisplayName);
}

void ANPCCharacter::Interact_Implementation(AActor* Interactor)
{
	if (IsValid(Interactor))
	{
		// Turn to whoever is talking; only the yaw, a villager doesn't lean.
		const FVector ToPlayer = Interactor->GetActorLocation() - GetActorLocation();
		SetActorRotation(FRotator(0.0f, ToPlayer.Rotation().Yaw, 0.0f));
	}

	if (UQuestLogComponent* QuestLog = GetQuestLog())
	{
		QuestLog->NotifyTalkedTo(NpcId);
	}

	LastResult = FText::GetEmpty();
	ShowMainMenu();
}

void ANPCCharacter::AddOption(TArray<FAdventurePanelOption>& Options, const FText& OptionLabel, const ENPCMenuAction Action, const int32 Index, const bool bEnabled)
{
	Options.Add(FAdventurePanelOption(OptionLabel, bEnabled));

	FNPCMenuAction& MenuAction = MenuActions.AddDefaulted_GetRef();
	MenuAction.Action = Action;
	MenuAction.Index = Index;
}

void ANPCCharacter::OpenPage(const FText& Body, const TArray<FAdventurePanelOption>& Options)
{
	UAdventureComponent* Adventure = GetAdventure();

	if (!IsValid(Adventure))
	{
		return;
	}

	const FText FullBody = LastResult.IsEmpty()
		? Body
		: FText::Format(LOCTEXT("BodyWithResult", "{0}\n\n{1}"), LastResult, Body);

	LastResult = FText::GetEmpty();

	Adventure->OpenPanel(DisplayName, FullBody, Options, FAdventurePanelChoiceDelegate::CreateUObject(this, &ANPCCharacter::HandleMenuChoice));
}

void ANPCCharacter::HandleMenuChoice(const int32 OptionIndex)
{
	if (!MenuActions.IsValidIndex(OptionIndex))
	{
		return;
	}

	// Copied: showing the next page rebuilds Menu Actions.
	const FNPCMenuAction Chosen = MenuActions[OptionIndex];

	switch (Chosen.Action)
	{
	case ENPCMenuAction::MainMenu:
		ShowMainMenu();
		break;

	case ENPCMenuAction::ShowQuest:
		ShowQuestPage(Chosen.Index);
		break;

	case ENPCMenuAction::AcceptQuest:
		if (UQuestLogComponent* QuestLog = GetQuestLog())
		{
			if (Quests.IsValidIndex(Chosen.Index))
			{
				QuestLog->AcceptQuest(Quests[Chosen.Index]);
			}
		}

		ShowMainMenu();
		break;

	case ENPCMenuAction::TurnInQuest:
		if (UQuestLogComponent* QuestLog = GetQuestLog())
		{
			if (Quests.IsValidIndex(Chosen.Index) && QuestLog->TurnInQuest(Quests[Chosen.Index]))
			{
				LastResult = Quests[Chosen.Index]->CompletionText;
			}
		}

		ShowMainMenu();
		break;

	case ENPCMenuAction::ShowShop:
		ShowShopPage();
		break;

	case ENPCMenuAction::BuyItem:
		BuyItem(Chosen.Index);
		ShowShopPage();
		break;

	case ENPCMenuAction::ShowSell:
		ShowSellPage();
		break;

	case ENPCMenuAction::SellStack:
		SellStack(Chosen.Index);
		ShowSellPage();
		break;

	case ENPCMenuAction::ShowForge:
		ShowForgePage();
		break;

	case ENPCMenuAction::Enhance:
		Enhance(Chosen.Index);
		ShowForgePage();
		break;

	default:
		if (UAdventureComponent* Adventure = GetAdventure())
		{
			Adventure->ClosePanel();
		}
		break;
	}
}

/* -------------------- Pages -------------------- */

void ANPCCharacter::ShowMainMenu()
{
	TArray<FAdventurePanelOption> Options;
	MenuActions.Reset();

	if (const UQuestLogComponent* QuestLog = GetQuestLog())
	{
		for (int32 QuestIndex = 0; QuestIndex < Quests.Num(); ++QuestIndex)
		{
			const UQuestDataAsset* Quest = Quests[QuestIndex];

			if (!IsValid(Quest))
			{
				continue;
			}

			switch (QuestLog->GetQuestState(Quest))
			{
			case EQuestState::ReadyToTurnIn:
				AddOption(Options, FText::Format(LOCTEXT("QuestReady", "[?]  {0}  (complete)"), Quest->Title), ENPCMenuAction::ShowQuest, QuestIndex);
				break;

			case EQuestState::Available:
				AddOption(Options, FText::Format(LOCTEXT("QuestAvailable", "[!]  {0}"), Quest->Title), ENPCMenuAction::ShowQuest, QuestIndex);
				break;

			case EQuestState::Active:
				AddOption(Options, FText::Format(LOCTEXT("QuestActive", "{0}  (in progress)"), Quest->Title), ENPCMenuAction::ShowQuest, QuestIndex);
				break;

			default:
				break;
			}
		}
	}

	if (!ShopItems.IsEmpty())
	{
		AddOption(Options, LOCTEXT("Shop", "Show me your wares"), ENPCMenuAction::ShowShop);
	}

	if (bBuysItems)
	{
		AddOption(Options, LOCTEXT("Sell", "I have things to sell"), ENPCMenuAction::ShowSell);
	}

	if (bBlacksmith)
	{
		AddOption(Options, LOCTEXT("Forge", "Enhance my equipment"), ENPCMenuAction::ShowForge);
	}

	AddOption(Options, LOCTEXT("Goodbye", "Goodbye"), ENPCMenuAction::Close);

	OpenPage(Greeting, Options);
}

void ANPCCharacter::ShowQuestPage(const int32 QuestIndex)
{
	const UQuestLogComponent* QuestLog = GetQuestLog();
	const UQuestDataAsset* Quest = Quests.IsValidIndex(QuestIndex) ? Quests[QuestIndex].Get() : nullptr;

	if (!IsValid(QuestLog) || !IsValid(Quest))
	{
		ShowMainMenu();
		return;
	}

	const EQuestState State = QuestLog->GetQuestState(Quest);

	FString Body = FString::Printf(TEXT("%s\n\n%s\n\n%s"),
		*Quest->Title.ToString(),
		*(State == EQuestState::ReadyToTurnIn && !Quest->CompletionText.IsEmpty() ? Quest->CompletionText : Quest->Description).ToString(),
		*QuestLog->BuildObjectivesText(Quest).ToString());

	// What it pays, in one line.
	TArray<FString> Rewards;

	if (Quest->ExperienceReward > 0)
	{
		Rewards.Add(FString::Printf(TEXT("%d experience"), Quest->ExperienceReward));
	}

	if (Quest->GoldReward > 0)
	{
		Rewards.Add(FString::Printf(TEXT("%d gold"), Quest->GoldReward));
	}

	for (const FInventorySlot& Reward : Quest->ItemRewards)
	{
		if (!Reward.IsEmpty())
		{
			Rewards.Add(Reward.Count > 1
				? FString::Printf(TEXT("%s x%d"), *Reward.Item->GetDisplayName().ToString(), Reward.Count)
				: Reward.Item->GetDisplayName().ToString());
		}
	}

	if (!Rewards.IsEmpty())
	{
		Body += TEXT("\n\nRewards: ") + FString::Join(Rewards, TEXT(", "));
	}

	TArray<FAdventurePanelOption> Options;
	MenuActions.Reset();

	if (State == EQuestState::Available)
	{
		AddOption(Options, LOCTEXT("Accept", "I'll do it"), ENPCMenuAction::AcceptQuest, QuestIndex);
	}
	else if (State == EQuestState::ReadyToTurnIn)
	{
		AddOption(Options, LOCTEXT("TurnIn", "It is done"), ENPCMenuAction::TurnInQuest, QuestIndex);
	}

	AddOption(Options, LOCTEXT("Back", "Back"), ENPCMenuAction::MainMenu);

	OpenPage(FText::FromString(Body), Options);
}

void ANPCCharacter::ShowShopPage()
{
	const UAdventureComponent* Adventure = GetAdventure();
	const int32 Gold = IsValid(Adventure) ? Adventure->GetGold() : 0;

	TArray<FAdventurePanelOption> Options;
	MenuActions.Reset();

	for (int32 ShopIndex = 0; ShopIndex < ShopItems.Num(); ++ShopIndex)
	{
		const FShopEntry& Entry = ShopItems[ShopIndex];

		if (!IsValid(Entry.Item))
		{
			continue;
		}

		const FText OptionLabel = Entry.Count > 1
			? FText::Format(LOCTEXT("BuyMany", "{0} x{1}   -   {2} gold"), Entry.Item->GetDisplayName(), Entry.Count, Entry.Price)
			: FText::Format(LOCTEXT("BuyOne", "{0}   -   {1} gold"), Entry.Item->GetDisplayName(), Entry.Price);

		AddOption(Options, OptionLabel, ENPCMenuAction::BuyItem, ShopIndex, Gold >= Entry.Price);
	}

	AddOption(Options, LOCTEXT("Back", "Back"), ENPCMenuAction::MainMenu);

	OpenPage(FText::Format(LOCTEXT("ShopBody", "You have {0} gold."), Gold), Options);
}

void ANPCCharacter::ShowSellPage()
{
	const UAdventureComponent* Adventure = GetAdventure();
	const UInventoryComponent* Inventory = IsValid(Adventure) ? Adventure->GetInventory() : nullptr;

	TArray<FAdventurePanelOption> Options;
	MenuActions.Reset();

	if (IsValid(Inventory))
	{
		for (int32 BagIndex = 0; BagIndex < Inventory->GetNumSlots(); ++BagIndex)
		{
			const FInventorySlot Stack = Inventory->GetSlot(BagIndex);

			if (Stack.IsEmpty() || Stack.Item->GetSellValue() <= 0)
			{
				continue;
			}

			const int32 Total = Stack.Item->GetSellValue() * Stack.Count;

			const FText OptionLabel = Stack.Count > 1
				? FText::Format(LOCTEXT("SellMany", "{0} x{1}   -   {2} gold"), Stack.Item->GetDisplayName(), Stack.Count, Total)
				: FText::Format(LOCTEXT("SellOne", "{0}   -   {1} gold"), Stack.Item->GetDisplayName(), Total);

			AddOption(Options, OptionLabel, ENPCMenuAction::SellStack, BagIndex);
		}
	}

	const bool bNothingToSell = MenuActions.IsEmpty();

	AddOption(Options, LOCTEXT("Back", "Back"), ENPCMenuAction::MainMenu);

	OpenPage(bNothingToSell
		? LOCTEXT("SellNothing", "You carry nothing I would buy.")
		: FText::Format(LOCTEXT("SellBody", "You have {0} gold. Choose what to sell; the whole stack goes."), IsValid(Adventure) ? Adventure->GetGold() : 0),
		Options);
}

void ANPCCharacter::ShowForgePage()
{
	const UAdventureComponent* Adventure = GetAdventure();
	const UInventoryComponent* Inventory = IsValid(Adventure) ? Adventure->GetInventory() : nullptr;
	const UEnhancementComponent* Enhancement = IsValid(Adventure) ? Adventure->GetEnhancement() : nullptr;
	const UAdventureSettings* Settings = UAdventureSettings::Get();

	TArray<FAdventurePanelOption> Options;
	MenuActions.Reset();

	if (IsValid(Inventory) && IsValid(Enhancement))
	{
		for (int32 EquipmentIndex = 0; EquipmentIndex < Inventory->GetNumEquipmentSlots(); ++EquipmentIndex)
		{
			const UEquipmentDataAsset* Worn = Inventory->GetEquippedItem(EquipmentIndex);

			if (!IsValid(Worn))
			{
				continue;
			}

			const FGameplayTag SlotTag = Inventory->GetEquipmentSlotDefinition(EquipmentIndex).SlotTag;
			const int32 Level = Enhancement->GetEnhancementLevel(SlotTag);

			if (Level >= Settings->MaxEnhancementLevel)
			{
				AddOption(Options, FText::Format(LOCTEXT("ForgeMax", "{0} +{1}   (as strong as it gets)"), Worn->GetDisplayName(), Level),
					ENPCMenuAction::Enhance, EquipmentIndex, false);
				continue;
			}

			AddOption(Options,
				FText::Format(LOCTEXT("ForgeOption", "{0} +{1}  ->  +{2}   -   {3} gold, {4}% chance"),
					Worn->GetDisplayName(), Level, Level + 1, Enhancement->GetNextLevelCost(SlotTag),
					FMath::RoundToInt(Enhancement->GetNextLevelChance(SlotTag))),
				ENPCMenuAction::Enhance, EquipmentIndex, Enhancement->CanEnhance(SlotTag));
		}
	}

	const bool bNothingWorn = MenuActions.IsEmpty();

	AddOption(Options, LOCTEXT("Back", "Back"), ENPCMenuAction::MainMenu);

	FString Body = bNothingWorn
		? FString(TEXT("You wear nothing I could work on."))
		: FString::Printf(TEXT("The enhancement stays with the slot: whatever you wear there gets its stats raised by %d%% per level.\n\nYou have %d gold."),
			FMath::RoundToInt(Settings->EnhancementBonusPerLevel * 100.0f), IsValid(Adventure) ? Adventure->GetGold() : 0);

	if (const UItemDataAsset* Material = Settings->EnhancementMaterial.LoadSynchronous())
	{
		Body += FString::Printf(TEXT("\nEach attempt also takes %d x %s (you have %d)."),
			Settings->EnhancementMaterialsPerAttempt, *Material->GetDisplayName().ToString(),
			IsValid(Inventory) ? Inventory->GetItemCount(Material) : 0);
	}

	OpenPage(FText::FromString(Body), Options);
}

/* -------------------- Trade -------------------- */

void ANPCCharacter::BuyItem(const int32 ShopIndex)
{
	UAdventureComponent* Adventure = GetAdventure();

	if (!IsValid(Adventure) || !ShopItems.IsValidIndex(ShopIndex) || !IsValid(ShopItems[ShopIndex].Item))
	{
		return;
	}

	const FShopEntry& Entry = ShopItems[ShopIndex];

	if (Adventure->GetGold() < Entry.Price || !Adventure->SpendGold(Entry.Price))
	{
		LastResult = LOCTEXT("CannotAfford", "You can't afford that.");
		return;
	}

	Adventure->GiveItem(Entry.Item, Entry.Count);
	LastResult = FText::Format(LOCTEXT("Bought", "Bought {0}."), Entry.Item->GetDisplayName());
}

void ANPCCharacter::SellStack(const int32 BagIndex)
{
	UAdventureComponent* Adventure = GetAdventure();
	UInventoryComponent* Inventory = IsValid(Adventure) ? Adventure->GetInventory() : nullptr;

	if (!IsValid(Inventory))
	{
		return;
	}

	const FInventorySlot Stack = Inventory->GetSlot(BagIndex);

	if (Stack.IsEmpty() || Stack.Item->GetSellValue() <= 0)
	{
		return;
	}

	const int32 Removed = Inventory->RemoveItemAt(BagIndex, Stack.Count);

	if (Removed > 0)
	{
		Adventure->GiveGold(Stack.Item->GetSellValue() * Removed);
		LastResult = FText::Format(LOCTEXT("Sold", "Sold {0}."), Stack.Item->GetDisplayName());
	}
}

void ANPCCharacter::Enhance(const int32 EquipmentIndex)
{
	const UAdventureComponent* Adventure = GetAdventure();
	const UInventoryComponent* Inventory = IsValid(Adventure) ? Adventure->GetInventory() : nullptr;
	UEnhancementComponent* Enhancement = IsValid(Adventure) ? Adventure->GetEnhancement() : nullptr;

	if (!IsValid(Inventory) || !IsValid(Enhancement))
	{
		return;
	}

	const FGameplayTag SlotTag = Inventory->GetEquipmentSlotDefinition(EquipmentIndex).SlotTag;

	switch (Enhancement->TryEnhance(SlotTag))
	{
	case EEnhanceResult::Success:
		LastResult = FText::Format(LOCTEXT("EnhanceSuccess", "The metal sings. It is now +{0}."), Enhancement->GetEnhancementLevel(SlotTag));
		break;

	case EEnhanceResult::Failed:
		LastResult = LOCTEXT("EnhanceFailed", "The enhancement failed. The gold and materials are lost.");
		break;

	default:
		LastResult = LOCTEXT("EnhanceNotPossible", "That can't be enhanced right now.");
		break;
	}
}

#undef LOCTEXT_NAMESPACE
