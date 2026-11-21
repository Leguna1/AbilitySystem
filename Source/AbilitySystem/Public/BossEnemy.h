#pragma once

#include "CoreMinimal.h"
#include "BaseEnemy.h"
#include "ModifierTypes.h"
#include "ResourceComponent.h"
#include "BossEnemy.generated.h"

/** A turn in a boss fight: at this much health, the boss changes. */
USTRUCT(BlueprintType)
struct FBossPhase
{
	GENERATED_BODY()

	/** The phase starts when health drops to this fraction of max (0.5 = half health). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float HealthFraction = 0.5f;

	/** Shown to the player when the phase starts, e.g. "The Flame Warden burns hotter!". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FText Announcement;

	/** Name of the effect on the boss's effect bar. Phases with different names stack; the same name replaces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FName EffectName = TEXT("Enraged");

	/** What changes for the rest of the fight: +30% Stat.AttackSpeed, +25% Stat.Damage, +20% Stat.MoveSpeed... */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	TArray<FStatModifier> Modifiers;

	/** Abilities with these ids are added to the boss's Attacks list from this phase on (set them up in Phase Attacks). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	bool bUnlockPhaseAttacks = false;
};

/**
 * A boss: an enemy that is fought once, shows its health across the top of the screen, changes as the
 * fight goes on, and counts toward winning the game.
 *
 * Make a Blueprint child like any enemy (mesh, attributes, abilities, Attacks), then:
 *   - Boss Id: its world flag, e.g. Boss.Fire. A dead boss stays dead, also after loading a save.
 *     Put the same name in a gate's Required Flags to open the way onward when it dies.
 *   - Phases: what changes at which health. Phase Attacks are extra attacks that join when a phase
 *     with Unlock Phase Attacks starts (grant those abilities to the boss like its other ones).
 *   - Place it in the level by hand (not through a spawner): bosses don't respawn.
 *
 * Tip: give it a large Leash Radius and Aggro Radius, plenty of Max Poise, and its Experience Reward.
 */
UCLASS(Abstract)
class ABILITYSYSTEM_API ABossEnemy : public ABaseEnemy
{
	GENERATED_BODY()

public:
	ABossEnemy();

	UFUNCTION(BlueprintPure, Category = "Boss")
	FName GetBossId() const { return BossId; }

	/** Index of the last phase that started; -1 before the first. */
	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetCurrentPhase() const { return CurrentPhase; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleCharacterDied(AActor* Killer) override;

	/** For Blueprint extras when a phase starts: a roar, an effect, adds spawning. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss")
	void OnPhaseStarted(int32 PhaseIndex);

	/** The world flag set when the boss dies. Must be unique per boss. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FName BossId = TEXT("Boss.One");

	/** Second line of the banner when it dies, e.g. "The seal on the eastern pass breaks." */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FText DefeatText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss", meta = (TitleProperty = "Announcement"))
	TArray<FBossPhase> Phases;

	/** Added to Attacks when a phase with Unlock Phase Attacks starts. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss", meta = (TitleProperty = "AbilityTag"))
	TArray<FAIAttack> PhaseAttacks;

private:
	UFUNCTION()
	void HandleResourceChanged(EResourceType ResourceType, EResourceValueType ValueType, float OldValue, float NewValue);

	/** Shows or hides the boss bar as the fight starts and ends. */
	void UpdateBossBar();

	void StartPhase(int32 PhaseIndex);

	TArray<bool> PhaseStarted;
	int32 CurrentPhase = INDEX_NONE;
	bool bPhaseAttacksUnlocked = false;
	bool bBossBarShown = false;
	FTimerHandle BossBarTimer;
};
