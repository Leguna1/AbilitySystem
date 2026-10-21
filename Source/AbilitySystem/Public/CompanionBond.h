#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CompanionBond.generated.h"

/**
 * What a companion has learned about its leader: how much, over all the fighting they've done together,
 * the leader tended to need each kind of help (indexed by ECompanionPurpose, 0..1 each).
 * Hidden from the player; it only shows in how the companion behaves.
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEM_API FCompanionBond
{
	GENERATED_BODY()

	/** Long-run average need per purpose, during fights. */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Companion|Bond")
	TArray<float> Habits;

	/** Seconds of fighting observed so far. Early on, habits move faster (first impressions). */
	UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Companion|Bond")
	float FightSecondsObserved = 0.0f;
};

/** Where the bond is kept between sessions, until a full save system takes it over (Get/Set Bond on the Companion Component). */
UCLASS()
class ABILITYSYSTEM_API UCompanionBondSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	FCompanionBond Bond;
};
