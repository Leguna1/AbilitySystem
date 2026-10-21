#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CompanionAIController.h"
#include "CompanionBond.h"
#include "CompanionComponent.generated.h"

class ACharacter;
class ACompanionCharacter;
class UAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCompanionChangedSignature, ACompanionCharacter*, Companion);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCompanionHabitShiftedSignature, ECompanionPurpose, NewStrongestHabit);

/** "While the leader has Required Ability learned, the companion has Companion Ability." */
USTRUCT(BlueprintType)
struct FCompanionAbilityGrant
{
	GENERATED_BODY()

	/** The leader's skill (a summon path, an upgrade). Empty = always granted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TSubclassOf<UAbility> RequiredAbility;

	/** Given to the companion: an attack for its bar, or a passive (Feral Bond). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	TSubclassOf<UAbility> CompanionAbility;

	/** The companion's ability has the same rank as the leader's Required Ability. Off = its own starting rank. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion")
	bool bRankFollowsRequiredAbility = true;
};

/**
 * The leader's side of a companion. Put it on the player.
 *   - Spawns it and tracks it; starts the summon cooldown when it dies.
 *   - Keeps its level in step with the leader's.
 *   - Optional Ability Grants: extra companion abilities tied to the leader's learned skills. Leave empty
 *     when the companion only uses its own kit.
 *   - Recall: the companion drops its fight and comes back.
 *   - The bond: what the companion has learned about you over all your fights (hidden, saved). It survives
 *     deaths and re-summons because it lives here, on you.
 * The companion decides for itself what to do (its Tactics). The HUD binds On Companion Changed to show
 * (or hide) the companion's health and bar.
 */
UCLASS(ClassGroup = (Companion), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UCompanionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompanionComponent();

	/** The living companion, or null. */
	UFUNCTION(BlueprintPure, Category = "Companion")
	ACompanionCharacter* GetCompanion() const;

	UFUNCTION(BlueprintPure, Category = "Companion")
	bool HasCompanion() const;

	/**
	 * Spawns CompanionClass at Location (feet), replacing any previous companion.
	 * Level = the leader's level + LevelOffset. When it dies, SourceAbilityId goes on its own cooldown
	 * (Cooldown Duration with the leader's Stat.Cooldown modifiers): the respawn wait.
	 */
	UFUNCTION(BlueprintCallable, Category = "Companion")
	ACompanionCharacter* SpawnCompanion(TSubclassOf<ACompanionCharacter> CompanionClass, FVector Location, int32 LevelOffset,
		const TArray<FCompanionAbilityGrant>& AbilityGrants, FGameplayTag SourceAbilityId);

	/**
	 * A spot for a companion near Leader, as the ground point (feet). Tries directions around PreferredDirection
	 * (its sides, behind, in front...) and takes the first that's reachable from Leader without a wall in between
	 * and has walkable ground near the leader's height (so not off a platform's edge, not on the floor below it).
	 * Snaps to the nav mesh nearby when it can. Falls back to the leader's own feet.
	 */
	static FVector FindPlacementNearLeader(const ACharacter* Leader, FVector PreferredDirection, float Distance);

	/* -------------------- Revive -------------------- */

	/** The fallen companion's body, while it lies there (its Corpse Lifetime). Null otherwise. */
	UFUNCTION(BlueprintPure, Category = "Companion|Revive")
	ACompanionCharacter* GetCompanionCorpse() const;

	/** A body to revive, within MaxDistance of the leader (0 = any distance). */
	UFUNCTION(BlueprintPure, Category = "Companion|Revive")
	bool CanReviveCompanion(float MaxDistance = 0.0f) const;

	/**
	 * Brings the fallen companion back where it fell, with HealthFraction of its max health,
	 * the same level and the same skills. Ends the summon's respawn cooldown.
	 */
	UFUNCTION(BlueprintCallable, Category = "Companion|Revive")
	ACompanionCharacter* ReviveCompanion(float HealthFraction = 0.5f);

	/** Removes the companion without a death (no respawn cooldown). */
	UFUNCTION(BlueprintCallable, Category = "Companion")
	void DismissCompanion();

	/** Drop the fight and come back. Summon pressed while the companion is out does this. */
	UFUNCTION(BlueprintCallable, Category = "Companion")
	void Recall();

	/* -------------------- Bond (learned habits) -------------------- */

	/**
	 * Called by the companion during fights with what you need right now (per purpose, 0..1).
	 * Slowly folds it into the long-run habits.
	 */
	void ObserveNeeds(const TArray<float>& CurrentNeeds, float DeltaSeconds);

	/** How much the bond favors Purpose: 1 = neutral, between Min Bias and Max Bias. */
	UFUNCTION(BlueprintPure, Category = "Companion|Bond")
	float GetBondBias(ECompanionPurpose Purpose) const;

	/** The long-run average need for Purpose, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Companion|Bond")
	float GetHabit(ECompanionPurpose Purpose) const;

	/** The whole bond, for a save system. */
	UFUNCTION(BlueprintPure, Category = "Companion|Bond")
	const FCompanionBond& GetBond() const { return Bond; }

	/** Restores a bond from a save system. */
	UFUNCTION(BlueprintCallable, Category = "Companion|Bond")
	void SetBond(const FCompanionBond& InBond);

	/** Forget everything learned (a new companion, a debug key). */
	UFUNCTION(BlueprintCallable, Category = "Companion|Bond")
	void ResetBond();

	/** Writes the bond to Bond Save Slot now. */
	UFUNCTION(BlueprintCallable, Category = "Companion|Bond")
	void SaveBond();

	/** Reads the bond from Bond Save Slot, if there is one. True if it loaded. */
	UFUNCTION(BlueprintCallable, Category = "Companion|Bond")
	bool LoadBond();

	/** One line of habits and biases, for on-screen debugging. */
	FString DescribeBond() const;

	/* -------------------- Events -------------------- */

	/** A companion was spawned, died or was dismissed (null). */
	UPROPERTY(BlueprintAssignable, Category = "Companion")
	FCompanionChangedSignature OnCompanionChanged;

	/** The companion's strongest habit changed (it has come to see you differently). Good for a bark or a look. */
	UPROPERTY(BlueprintAssignable, Category = "Companion|Bond")
	FCompanionHabitShiftedSignature OnHabitShifted;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Seconds of fighting for a habit to mostly settle on a new pattern. Large = it takes many fights
	 * to change its mind. The first fights count more (first impressions), down to this pace.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Bond", meta = (ClampMin = "10", Units = "s"))
	float HabitLearnTime = 900.0f;

	/** How strongly habits sway it: a habit 0.3 above the average habit gives 1 + 0.3 x 2 x Bias Strength. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Bond", meta = (ClampMin = "0.0"))
	float BiasStrength = 1.0f;

	/** Never favors a purpose less than this, so it never stops doing anything entirely. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Bond", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float MinBias = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Bond", meta = (ClampMin = "1.0"))
	float MaxBias = 1.6f;

	/** Keep the bond in its own save slot (loaded at start, saved during fights and on exit). Off = a save system handles it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Bond")
	bool bUseBondSaveSlot = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Bond", meta = (EditCondition = "bUseBondSaveSlot"))
	FString BondSaveSlot = TEXT("CompanionBond");

	/** Saves after this many seconds of observed fighting since the last save. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Bond", meta = (ClampMin = "5", Units = "s", EditCondition = "bUseBondSaveSlot"))
	float BondSaveInterval = 30.0f;

private:
	UFUNCTION()
	void HandleCompanionDied(AActor* Killer);

	UFUNCTION()
	void HandleLeaderLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleLeaderAbilityRankChanged(FGameplayTag AbilityId, int32 OldRank, int32 NewRank);

	/** Grants, revokes and re-ranks the companion's abilities to match the leader's learned skills. */
	void ApplyAbilityGrants();

	ACompanionAIController* GetCompanionController() const;
	int32 GetLeaderLevel() const;
	void ReleaseCompanion();

	TWeakObjectPtr<ACompanionCharacter> Companion;

	/** The body after a death, until it decays or a new companion is summoned. */
	TWeakObjectPtr<ACompanionCharacter> Corpse;

	/** What was summoned last, for a revive. */
	UPROPERTY(Transient)
	TSubclassOf<ACompanionCharacter> LastCompanionClass;

	void ClearCorpse();

	UPROPERTY(Transient)
	TArray<FCompanionAbilityGrant> ActiveGrants;

	int32 CompanionLevelOffset = 0;

	/** What the companion has learned about this leader. */
	UPROPERTY(Transient)
	FCompanionBond Bond;

	/** Async during play (no hitch); blocking on exit, so it's written before the game closes. */
	void WriteBondToSlot(bool bAsync);

	/** Fills missing habits with the neutral starting value. */
	void EnsureBondSize();

	/** Purpose with the highest habit, or none while habits are too even to call. */
	int32 FindStrongestHabit() const;

	int32 StrongestHabit = INDEX_NONE;
	float FightSecondsSinceSave = 0.0f;
	FGameplayTag RespawnAbilityId;
};
