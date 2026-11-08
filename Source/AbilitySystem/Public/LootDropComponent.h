#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.h"
#include "LootDropComponent.generated.h"

class ALootPickup;
class UItemDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLootDroppedSignature, const TArray<ALootPickup*>&, Pickups);

/** One item this actor can drop, and how its chance differs from the item's own Drop Chance. */
USTRUCT(BlueprintType)
struct FLootDrop
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UItemDataAsset> Item;

	/**
	 * Off = the item's own Drop Chance times Drop Chance Multiplier.
	 * On = ignore the item's chance and use Drop Chance here (100 for a guaranteed boss drop).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bOverrideDropChance = false;

	/** Scales the item's own Drop Chance for this actor. 2 = twice as likely, 0.5 = half. The result is capped at 100%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", EditCondition = "!bOverrideDropChance", EditConditionHides))
	float DropChanceMultiplier = 1.0f;

	/** Percent chance used instead of the item's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "Percent", EditCondition = "bOverrideDropChance", EditConditionHides))
	float DropChance = 100.0f;

	/** How many drop when it drops, picked at random between Min and Max. For a currency (Gold), the coin amount. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};

/**
 * What an actor drops when it dies. Each entry in Loot is rolled on its own, so one kill can drop
 * nothing, one item or several. Dropped items lie on the ground as Loot Pickups around the body;
 * anyone with an Inventory Component walking over them picks them up.
 *
 * Every enemy has one. It works on anything with a Combatant Component (a breakable crate),
 * or call Drop Loot yourself (a chest being opened) with Drop On Death off.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API ULootDropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULootDropComponent();

	/** The entry's final percent chance on this actor: item chance or override, times the multipliers, capped at 100. */
	UFUNCTION(BlueprintPure, Category = "Loot")
	float GetFinalDropChance(const FLootDrop& Entry) const;

	/** Rolls every entry once and returns what came up, without spawning anything. */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	TArray<FInventorySlot> RollLoot() const;

	/** Rolls the loot and spawns a pickup for each result, scattered on the ground around the owner. */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	TArray<ALootPickup*> DropLoot();

	/** Spawns a pickup for Item on the ground near Origin. Null if nothing could be spawned. */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	ALootPickup* SpawnPickup(UItemDataAsset* Item, int32 Count, FVector Origin);

	/** Loot hit the ground. Only fires when at least one item dropped. */
	UPROPERTY(BlueprintAssignable, Category = "Loot")
	FLootDroppedSignature OnLootDropped;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Everything this can drop. Each entry is rolled separately. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (TitleProperty = "Item"))
	TArray<FLootDrop> Loot;

	/**
	 * Scales every entry's chance on this actor, overrides included. Use it for variants:
	 * an elite wolf at 2 drops twice as often as a normal one with the same list.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0"))
	float LootChanceMultiplier = 1.0f;

	/** Rolls and drops the loot when the owner's Combatant Component reports its death. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	bool bDropOnDeath = true;

	/** Pickups land at random points this far from the owner, so several drops don't stack on one spot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Spawning", meta = (ClampMin = "0.0", Units = "cm"))
	float ScatterRadius = 120.0f;

	/** Pickup spawned for each drop. Empty = Project Settings -> Ability System -> Default Loot Pickup Class. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot|Spawning")
	TSubclassOf<ALootPickup> PickupClass;

private:
	UFUNCTION()
	void HandleOwnerDied(AActor* Killer);

	/** Component setting, then the project default, then the plain C++ pickup. */
	TSubclassOf<ALootPickup> ResolvePickupClass() const;

	/** A point on the ground near Origin, a random distance up to Scatter Radius away. */
	FVector FindDropLocation(const FVector& Origin) const;
};
