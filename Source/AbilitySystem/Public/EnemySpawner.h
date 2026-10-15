#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class USphereComponent;

/**
 * Keeps a group of enemies alive around this spot: spawns them, notices when they die,
 * clears the corpses and brings them back after a delay. Place one per pack (e.g. 4 golems).
 *
 * Each enemy spawns on the navmesh inside Spawn Radius, so its home (leash, roaming) is there.
 * The pink sphere in the editor shows the area; it's invisible in game.
 */
UCLASS(Blueprintable)
class ABILITYSYSTEM_API AEnemySpawner : public AActor
{
	GENERATED_BODY()

public:
	AEnemySpawner();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Spawns every enemy that isn't alive right now. */
	UFUNCTION(BlueprintCallable, Category = "Spawner")
	void SpawnMissing();

	/** Enemies of this spawner that are alive. */
	UFUNCTION(BlueprintPure, Category = "Spawner")
	int32 GetAliveCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** A spawned enemy is in the world (and possessed by its AI). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawner")
	void OnEnemySpawned(APawn* SpawnedEnemy);

	/** One of this spawner's enemies died. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawner")
	void OnEnemyDied(APawn* DeadEnemy);

	/** What to spawn, e.g. BP_Golem. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner")
	TSubclassOf<APawn> EnemyClass;

	/** Enemies kept alive by this spawner. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "1", ClampMax = "32"))
	int32 Count = 3;

	/** Enemies appear at random reachable points within this distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (ClampMin = "0.0", Units = "cm"))
	float SpawnRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner")
	bool bSpawnOnBeginPlay = true;

	/** Seconds after a death before that enemy comes back. 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Respawn", meta = (ClampMin = "0.0", Units = "s"))
	float RespawnDelay = 30.0f;

	/** Wait until the whole group is dead, then bring all of them back together (a pack returns as a pack). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Respawn")
	bool bRespawnAsGroup = false;

	/** Seconds a corpse stays before it's removed. 0 = it stays until its enemy respawns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawner|Respawn", meta = (ClampMin = "0.0", Units = "s"))
	float CorpseLifetime = 10.0f;

	/** Editor-only view of the spawn area. */
	UPROPERTY(VisibleAnywhere, Category = "Spawner")
	TObjectPtr<USphereComponent> SpawnArea;

private:
	struct FSpawnSlot
	{
		TWeakObjectPtr<APawn> Enemy;

		/** World time it was found dead. Below 0 = alive (or never spawned). */
		double DiedAt = -1.0;
	};

	/** Checks the slots twice a second: deaths, corpses, respawns. */
	void UpdateSlots();

	void SpawnInSlot(int32 SlotIndex);
	bool FindSpawnLocation(FVector& OutLocation) const;

	static bool IsEnemyAlive(const APawn* SpawnedEnemy);

	TArray<FSpawnSlot> Slots;
	FTimerHandle UpdateTimer;
};
