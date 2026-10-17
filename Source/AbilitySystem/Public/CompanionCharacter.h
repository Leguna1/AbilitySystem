#pragma once

#include "CoreMinimal.h"
#include "BaseAICharacter.h"
#include "CompanionCharacter.generated.h"

class UActionBarComponent;

/**
 * A pet that fights for its leader (the wolf). Same team as the leader, so enemies attack it and
 * the leader's targeting ignores it. Joins fights its leader is in, follows otherwise, and passes
 * its kills' experience to the leader.
 *
 * Spawned by a Summon Companion Ability through the leader's Companion Component.
 * Attacks come from the Attacks list as for enemies; Command Only = auto-cast starts off (Pounce).
 * Its Action Bar Component is the companion bar the player sees: set 3-4 slots (labels "Shift 1"...).
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

	/** The companion bar: which of its abilities sit on which slot. */
	UFUNCTION(BlueprintPure, Category = "Companion")
	UActionBarComponent* GetCompanionActionBar() const { return CompanionActionBar; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	/** Seconds a hostile stays "in the fight" after it hit, or was hit by, the leader or the companion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion", meta = (ClampMin = "0", Units = "s"))
	float AssistMemory = 6.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UActionBarComponent> CompanionActionBar;

	/** Its kills give the leader experience, as if the leader had made them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Companion")
	bool bShareKillsWithLeader = true;

private:
	UFUNCTION()
	void HandleKilled(AActor* Victim);

	TWeakObjectPtr<ABaseCharacter> Leader;
};
