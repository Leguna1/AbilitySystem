#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldProgressSubsystem.generated.h"

class APawn;
class UAdventureSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWorldFlagSetSignature, FName, Flag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWorldVictorySignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWorldPlayerReadySignature, APawn*, PlayerPawn);

/** Something the game is won by: an altar to take or a boss to defeat. Done when its Flag is set. */
USTRUCT(BlueprintType)
struct FWorldObjective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "World Progress")
	FName Flag;

	UPROPERTY(BlueprintReadOnly, Category = "World Progress")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "World Progress")
	bool bBoss = false;
};

/**
 * What has happened in the world, as a list of flags (plain names): "Altar.Dawn" once that altar is taken,
 * "Boss.Fire" once that boss is dead, a chest's id once opened. Everything that reacts to progress reads
 * flags: a gate opens when its Required Flags are all set, a quest objective completes on a flag,
 * a boss that is already dead doesn't appear again.
 *
 * The game is won when every registered objective (altars and bosses register themselves) has its flag.
 *
 * Also loads the save before any actor begins play, so actors can just ask Has Flag in Begin Play,
 * and keeps the list of interactable actors the player looks through.
 */
UCLASS()
class ABILITYSYSTEM_API UWorldProgressSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Null outside a game world. */
	static UWorldProgressSubsystem* Get(const UObject* WorldContextObject);

	/** Set once every objective is done. */
	static const FName VictoryFlag;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/* -------------------- Flags -------------------- */

	/** Sets Flag (once: setting it again does nothing), tells listeners, checks for victory. */
	UFUNCTION(BlueprintCallable, Category = "World Progress")
	void SetFlag(FName Flag);

	UFUNCTION(BlueprintPure, Category = "World Progress")
	bool HasFlag(FName Flag) const;

	/** True when every flag in the list is set. An empty list is true. */
	UFUNCTION(BlueprintPure, Category = "World Progress")
	bool HasAllFlags(const TArray<FName>& InFlags) const;

	const TArray<FName>& GetWorldFlags() const { return WorldFlags; }

	UPROPERTY(BlueprintAssignable, Category = "World Progress")
	FWorldFlagSetSignature OnFlagSet;

	/* -------------------- Objectives and victory -------------------- */

	/** Called by altars and bosses in Begin Play. The game is won when all registered flags are set. */
	UFUNCTION(BlueprintCallable, Category = "World Progress")
	void RegisterObjective(FName Flag, FText DisplayName, bool bBoss);

	UFUNCTION(BlueprintPure, Category = "World Progress")
	const TArray<FWorldObjective>& GetObjectives() const { return Objectives; }

	UFUNCTION(BlueprintPure, Category = "World Progress")
	int32 GetCompletedObjectiveCount() const;

	UFUNCTION(BlueprintPure, Category = "World Progress")
	bool IsVictoryAchieved() const { return HasFlag(VictoryFlag); }

	/** Every altar is taken and every boss is dead. Fires once, at that moment. */
	UPROPERTY(BlueprintAssignable, Category = "World Progress")
	FWorldVictorySignature OnVictory;

	/* -------------------- Interactables -------------------- */

	/** Actor must implement Interactable Interface. Call from Begin Play, and Unregister from End Play. */
	void RegisterInteractable(AActor* Actor);
	void UnregisterInteractable(AActor* Actor);
	const TArray<TWeakObjectPtr<AActor>>& GetInteractables() const { return Interactables; }

	/* -------------------- Player -------------------- */

	/** Called by the Adventure Component once the player's pawn is set up (and its save applied). */
	void NotifyPlayerReady(APawn* PlayerPawn);

	/** The player's pawn once it is ready; null before that. */
	UFUNCTION(BlueprintPure, Category = "World Progress")
	APawn* GetReadyPlayerPawn() const { return ReadyPlayerPawn.Get(); }

	/** For actors that give the player something lasting and must give it again after loading (upgrade shrines). */
	UPROPERTY(BlueprintAssignable, Category = "World Progress")
	FWorldPlayerReadySignature OnPlayerReady;

	/* -------------------- Saving -------------------- */

	/** The save read when the world started; null if there was none or saving is off. */
	UAdventureSaveGame* GetLoadedSave() const { return LoadedSave; }

	/** Writes the world's flags and the player's state to the save slot. Does nothing when saving is off. */
	UFUNCTION(BlueprintCallable, Category = "World Progress")
	void SaveGame();

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void CheckVictory();

	TArray<FName> WorldFlags;

	UPROPERTY(Transient)
	TArray<FWorldObjective> Objectives;

	TArray<TWeakObjectPtr<AActor>> Interactables;
	TWeakObjectPtr<APawn> ReadyPlayerPawn;

	UPROPERTY(Transient)
	TObjectPtr<UAdventureSaveGame> LoadedSave;
};
