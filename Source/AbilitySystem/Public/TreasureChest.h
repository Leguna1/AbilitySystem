#pragma once

#include "CoreMinimal.h"
#include "AdventureProp.h"
#include "InventoryComponent.h"
#include "TreasureChest.generated.h"

class ULootDropComponent;
class UItemDataAsset;

/**
 * A chest opened once. Contents go straight into the bag (what doesn't fit lands at the player's feet);
 * the Loot Drop Component's list is rolled on top, for chests with a bit of luck in them.
 *
 * Optional: a Key Item to open it, and Required Flags that keep it shut until something is done
 * (the room's switch, a boss). Give every chest its own Chest Id so it stays opened after loading a save.
 */
UCLASS()
class ABILITYSYSTEM_API ATreasureChest : public AAdventureProp
{
	GENERATED_BODY()

public:
	ATreasureChest();

	UFUNCTION(BlueprintPure, Category = "Chest")
	bool IsOpened() const { return bOpened; }

	virtual FText GetInteractionPrompt_Implementation(AActor* Interactor) const override;
	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual void Interact_Implementation(AActor* Interactor) override;

protected:
	virtual void BeginPlay() override;

	/** For Blueprint looks: the lid opening, a jingle. Also fires (From Save) for chests already opened in a loaded game. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Chest")
	void OnChestOpened(bool bFromSave);

	/** World flag set when opened; a chest whose id is set starts opened. Must be unique per chest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
	FName ChestId;

	/** Always inside. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
	TArray<FInventorySlot> Contents;

	/** Gold inside, picked at random between Min and Max. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest", meta = (ClampMin = "0"))
	int32 GoldMin = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest", meta = (ClampMin = "0"))
	int32 GoldMax = 0;

	/** Item needed to open it. Empty = no lock. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest|Lock")
	TObjectPtr<UItemDataAsset> KeyItem;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest|Lock")
	bool bConsumeKey = true;

	/** World flags that must be set before it opens (a switch's Flag To Set, a Boss Id). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest|Lock")
	TArray<FName> RequiredFlags;

	/** Extra random loot: fill its Loot list (select the component). Rolled once when the chest opens. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULootDropComponent> LootDropComponent;

private:
	bool IsSealed() const;
	bool HasKey(const AActor* Interactor) const;
	void ApplyOpenedState(bool bFromSave);

	bool bOpened = false;
};
