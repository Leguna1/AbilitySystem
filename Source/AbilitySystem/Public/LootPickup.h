#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootPickup.generated.h"

class UItemDataAsset;
class URotatingMovementComponent;
class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

/**
 * An item lying on the ground. Walking over it puts it in the collector's Inventory Component.
 * If the bag can't take all of it, the rest stays on the ground.
 *
 * Make a Blueprint child for looks (glow, rarity beam, sound) and set it as the Default Loot Pickup Class
 * in Project Settings. The mesh comes from the item's Pickup Mesh when it has one.
 */
UCLASS()
class ABILITYSYSTEM_API ALootPickup : public AActor
{
	GENERATED_BODY()

public:
	ALootPickup();

	/** Sets what this pickup holds and shows it. Called by the Loot Drop Component right after spawning. */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	void InitializePickup(UItemDataAsset* InItem, int32 InCount);

	/**
	 * Puts as much as fits into Collector's inventory. Returns true when everything was taken
	 * (the pickup is then destroyed). False if it isn't collectable yet, Collector has no inventory, or the bag is full.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot")
	bool TryPickUp(AActor* Collector);

	UFUNCTION(BlueprintPure, Category = "Loot")
	UItemDataAsset* GetItem() const { return Item; }

	UFUNCTION(BlueprintPure, Category = "Loot")
	int32 GetCount() const { return Count; }

protected:
	virtual void BeginPlay() override;

	/** For Blueprint looks that depend on the item (a rarity color, a name label). Fires after Initialize Pickup. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot")
	void OnPickupInitialized(UItemDataAsset* PickupItem, int32 PickupCount);

	/** Some or all of it went into Collector's bag. Count Taken is how many; the rest (if any) stays. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Loot")
	void OnPickedUp(AActor* Collector, int32 CountTaken);

	/** Who walks over it picks it up. Only Pawns are noticed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> PickupSphere;

	/** Shows the item's Pickup Mesh; keeps its Blueprint mesh when the item has none. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Slow spin so loot catches the eye. Set its Rotation Rate to zero to stop it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<URotatingMovementComponent> Spin;

	/** Seconds after spawning before it can be picked up, so the player sees it drop instead of it vanishing at once. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", Units = "s"))
	float PickupDelay = 0.5f;

	/** Seconds it stays on the ground before disappearing. 0 = stays forever. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", Units = "s"))
	float Lifetime = 120.0f;

	/** Played at the pickup when anything is taken. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<USoundBase> PickupSound;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Loot")
	TObjectPtr<UItemDataAsset> Item;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Loot")
	int32 Count = 0;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** End of Pickup Delay: anyone already standing on it gets it now, since no new overlap will fire for them. */
	void EnablePickup();

	bool bCanBePickedUp = false;
	FTimerHandle PickupDelayTimer;
};
