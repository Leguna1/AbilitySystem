#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PayloadReceiver.h"
#include "Breakable.generated.h"

class ULootDropComponent;
class UStaticMeshComponent;

/**
 * Pots, crates, barrels: anything that breaks when hit and may drop something (a few coins, a potion).
 * Fill the Loot Drop Component's Loot list. With Respawn Delay set it comes back, like Zelda's pots.
 *
 * Arrows and other projectiles break it out of the box. Melee swings break it if the weapon's
 * melee trace also looks for this mesh's object type (World Static by default).
 */
UCLASS()
class ABILITYSYSTEM_API ABreakable : public AActor, public IPayloadReceiver
{
	GENERATED_BODY()

public:
	ABreakable();

	UFUNCTION(BlueprintPure, Category = "Breakable")
	bool IsBroken() const { return bBroken; }

	virtual bool ReceivePayload_Implementation(const FAbilityPayload& Payload) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** For Blueprint looks: shards, dust, a crash. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Breakable")
	void OnBroken(AActor* Breaker);

	UFUNCTION(BlueprintImplementableEvent, Category = "Breakable")
	void OnRestored();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** What falls out. Fill its Loot list (select the component). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULootDropComponent> LootDropComponent;

	/** Damaging hits it takes before breaking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakable", meta = (ClampMin = "1"))
	int32 HitsToBreak = 1;

	/** Seconds until it is whole again. 0 = gone for good. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Breakable", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnDelay = 0.0f;

private:
	void Break(AActor* Breaker);
	void Restore();

	int32 HitsTaken = 0;
	bool bBroken = false;
	FTimerHandle RespawnTimer;
};
