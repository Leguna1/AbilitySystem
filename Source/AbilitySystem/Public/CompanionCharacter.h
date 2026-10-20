#pragma once

#include "CoreMinimal.h"
#include "BaseAICharacter.h"
#include "CompanionTactics.h"
#include "CompanionCharacter.generated.h"

class UActionBarComponent;
class UCompanionNeedsComponent;

/**
 * A companion that fights beside its leader (the wolf), on its own judgement. Same team as the leader,
 * so enemies attack it and the leader's targeting ignores it. Passes its kills' experience to the leader.
 *
 * Spawned by a Summon Companion Ability through the leader's Companion Component. Its level follows the
 * leader's, so its own abilities grow with its attributes.
 *
 * What it does is taught through Tactics: one per ability, saying what it's for and when it's a good idea.
 * The Attacks list (from Base AI Character) is not used by companions.
 * Its Action Bar Component only displays its abilities and cooldowns; the player doesn't control it.
 */
UCLASS(Abstract)
class ABILITYSYSTEM_API ACompanionCharacter : public ABaseAICharacter
{
	GENERATED_BODY()

public:
	ACompanionCharacter();

	/** Joins Leader's team. Call before the companion begins play (the Companion Component does). */
	void SetLeader(ABaseCharacter* NewLeader);

	UFUNCTION(BlueprintPure, Category = "Companion")
	ABaseCharacter* GetLeader() const { return Leader.Get(); }

	UFUNCTION(BlueprintPure, Category = "Companion")
	float GetFollowDistance() const { return FollowDistance; }

	UFUNCTION(BlueprintPure, Category = "Companion")
	float GetTeleportDistance() const { return TeleportDistance; }

	UFUNCTION(BlueprintPure, Category = "Companion")
	float GetLeashRadius() const { return LeashRadius; }

	UFUNCTION(BlueprintPure, Category = "Companion")
	float GetAssistRadius() const { return AssistRadius; }

	UFUNCTION(BlueprintPure, Category = "Companion")
	float GetAssistMemory() const { return AssistMemory; }

	const TArray<FCompanionTactic>& GetTactics() const { return Tactics; }

	/** Reads what the leader needs right now; tactics are weighed by it. */
	UFUNCTION(BlueprintPure, Category = "Companion")
	UCompanionNeedsComponent* GetNeedsComponent() const { return NeedsComponent; }

	/** Also picks fights with enemies near the leader that haven't noticed you yet. */
	bool EngagesUnprovoked() const { return bEngageUnprovoked; }

	/** The companion bar the HUD shows (its abilities and cooldowns). Display only. */
	UFUNCTION(BlueprintPure, Category = "Companion")
	UActionBarComponent* GetCompanionActionBar() const { return CompanionActionBar; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** How it uses each of its abilities: what for, on whom, and when. See FCompanionTactic. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Tactics", meta = (TitleProperty = "AbilityTag"))
	TArray<FCompanionTactic> Tactics;

	/**
	 * Off (default): joins fights the leader or it are in: enemies going for you, ones you hit or lock on to.
	 * On: also attacks any enemy it can see near the leader, even ones that haven't noticed you.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion|Tactics")
	bool bEngageUnprovoked = false;

	/** Out of combat, stays within this distance of the leader. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion", meta = (ClampMin = "0", Units = "cm"))
	float FollowDistance = 300.0f;

	/** Farther than this from the leader (or no path), it appears beside them instead of running. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion", meta = (ClampMin = "0", Units = "cm"))
	float TeleportDistance = 2500.0f;

	/** Farther than this from the leader, it drops its target and comes back. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion", meta = (ClampMin = "0", Units = "cm"))
	float LeashRadius = 1800.0f;

	/** Joins fights with hostiles this close to the leader. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion", meta = (ClampMin = "0", Units = "cm"))
	float AssistRadius = 1500.0f;

	/** Seconds an enemy stays "in the fight" after it hit, or was hit by, the leader or the companion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion", meta = (ClampMin = "0", Units = "s"))
	float AssistMemory = 6.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActionBarComponent> CompanionActionBar;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCompanionNeedsComponent> NeedsComponent;

	/** Its kills give the leader experience, as if the leader had made them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion")
	bool bShareKillsWithLeader = true;

private:
	UFUNCTION()
	void HandleKilled(AActor* Victim);

	TWeakObjectPtr<ABaseCharacter> Leader;
};
