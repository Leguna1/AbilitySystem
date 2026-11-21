#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InteractableInterface.h"
#include "NPCCharacter.generated.h"

class UAdventureComponent;
class UItemDataAsset;
class UQuestDataAsset;
class UQuestLogComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
struct FAdventurePanelOption;

/** What an option on a villager's current page does. */
enum class ENPCMenuAction : uint8
{
	Close,
	MainMenu,
	ShowQuest,
	AcceptQuest,
	TurnInQuest,
	ShowShop,
	BuyItem,
	ShowSell,
	SellStack,
	ShowForge,
	Enhance
};

struct FNPCMenuAction
{
	ENPCMenuAction Action = ENPCMenuAction::Close;

	/** Which quest, shop entry, bag slot or equipment slot. */
	int32 Index = INDEX_NONE;
};

/** Something a merchant sells. */
USTRUCT(BlueprintType)
struct FShopEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UItemDataAsset> Item;

	/** Gold for one purchase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
	int32 Price = 10;

	/** How many one purchase gives (arrows by the bundle, potions one at a time). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "1"))
	int32 Count = 1;
};

/**
 * A villager. Depending on what you fill in, the same class is a quest giver, a merchant, a blacksmith,
 * or just someone with something to say:
 *   - Quests: offered when the player meets their requirements, handed in here too.
 *     A yellow ! floats above a villager with a quest to give, a ? above one waiting for a finished quest.
 *   - Shop Items: sold for gold. Buys Items: also buys anything with a Sell Value from the bag.
 *   - Blacksmith: enhances the player's equipment slots (see Enhancement Component).
 *   - Npc Id: quests with a Talk To objective name it.
 *
 * Place it in the level, or make a Blueprint child to give it a skeletal mesh and an idle animation
 * (select Mesh; set Anim Mode to Use Animation Asset and pick the idle). Until it has a mesh, a plain
 * pillar stands in so the villager can be seen and talked to.
 */
UCLASS()
class ABILITYSYSTEM_API ANPCCharacter : public ACharacter, public IInteractableInterface
{
	GENERATED_BODY()

public:
	ANPCCharacter();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "NPC")
	FName GetNpcId() const { return NpcId; }

	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override { return true; }
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Name used by Talk To quest objectives. Unique per villager that quests refer to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FName NpcId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FText DisplayName;

	/** Shown under the name, e.g. "Blacksmith". Optional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC")
	FText Title;

	/** What the villager says when talked to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC", meta = (MultiLine = "true"))
	FText Greeting;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Quests")
	TArray<TObjectPtr<UQuestDataAsset>> Quests;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Trade", meta = (TitleProperty = "Item"))
	TArray<FShopEntry> ShopItems;

	/** Buys anything in the player's bag that has a Sell Value. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Trade")
	bool bBuysItems = false;

	/** Offers to enhance the player's equipment slots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Trade")
	bool bBlacksmith = false;

	/** Stands in until the character has a skeletal mesh. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Placeholder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTextRenderComponent> NameLabel;

private:
	UFUNCTION()
	void HandleQuestLogChanged();

	void HandleMenuChoice(int32 OptionIndex);

	void ShowMainMenu();
	void ShowQuestPage(int32 QuestIndex);
	void ShowShopPage();
	void ShowSellPage();
	void ShowForgePage();

	void BuyItem(int32 ShopIndex);
	void SellStack(int32 BagIndex);
	void Enhance(int32 EquipmentIndex);

	/** Adds an option to the page being built and remembers what it does. */
	void AddOption(TArray<FAdventurePanelOption>& Options, const FText& OptionLabel, ENPCMenuAction Action, int32 Index = INDEX_NONE, bool bEnabled = true);

	/** Opens the panel with this villager's name as the title. Result (if any) is the first line: "Bought...", "Enhancement failed". */
	void OpenPage(const FText& Body, const TArray<FAdventurePanelOption>& Options);

	/** The ! or ? above the head, and the name under it. */
	void RefreshMarker();

	UAdventureComponent* GetAdventure() const;
	UQuestLogComponent* GetQuestLog() const;

	/** One per option on the page currently shown. */
	TArray<FNPCMenuAction> MenuActions;

	/** Outcome of the last purchase, sale or enhancement, shown at the top of the next page. */
	FText LastResult;

	TWeakObjectPtr<UQuestLogComponent> BoundQuestLog;
	FTimerHandle MarkerTimer;
};
