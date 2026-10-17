#pragma once

#include "CoreMinimal.h"
#include "MontageAbility.h"
#include "CompanionComponent.h"
#include "SummonCompanionAbility.generated.h"

class ACompanionCharacter;

/** What pressing Summon does while the companion is already out. */
UENUM(BlueprintType)
enum class ECompanionOutAction : uint8
{
	/** It drops its fight and comes back to you. */
	Recall,

	/** It's sent away (no respawn cooldown). */
	Dismiss
};

/**
 * Summons your companion: a skill tree skill of its own, outside any weapon kit (grant it through
 * the Ability Component's Starting Ability Classes), so it stays through weapon swaps.
 *
 * Its paths and upgrades are ordinary skill tree nodes. Ability Grants turn them into the companion's
 * abilities: "Pack Leader learned -> the wolf has Howl", ranked like the node.
 *
 * The companion's own abilities live on its companion bar (commands, auto-cast, stance).
 * Cooldown: its own Cooldown Duration is the respawn wait. Cooldown Trigger defaults to Manual, so summoning
 * and recalling cost no cooldown; the wait starts when the companion dies, with Stat.Cooldown modifiers
 * applied (a passive "-10 s companion cooldown" just works).
 * Montage optional (a whistle): without one it acts right away.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API USummonCompanionAbility : public UMontageAbility
{
	GENERATED_BODY()

public:
	USummonCompanionAbility();

	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityCommitted_Implementation() override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;

	/** True while the companion is out (the slot can show a Recall or Dismiss icon). */
	UFUNCTION(BlueprintPure, Category = "Ability|Companion")
	bool IsCompanionOut() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Companion")
	TSubclassOf<ACompanionCharacter> CompanionClass;

	/** The companion's abilities that come from your skills. Its base kit is on the companion itself. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Companion", meta = (TitleProperty = "CompanionAbility"))
	TArray<FCompanionAbilityGrant> AbilityGrants;

	/** Companion level = your level + this, by rank. Higher ranks bring a stronger companion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Companion")
	FAbilityRankedInt LevelOffsetByRank = FAbilityRankedInt(0);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Companion")
	ECompanionOutAction WhenAlreadyOut = ECompanionOutAction::Recall;

	/** It appears this far to your side. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Companion", meta = (ClampMin = "0", Units = "cm"))
	float SpawnDistance = 200.0f;

	/** Animation event at which it acts (the whistle's peak). Empty = when the ability commits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Companion")
	FGameplayTag ActEventTag;

	/** It acted. For Blueprint extras: a summon effect at the companion, a sound. Companion is null after a dismiss. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Companion")
	void OnCompanionAction(bool bSummoned, ACompanionCharacter* Companion);

private:
	void Act();
	FVector FindSpawnLocation() const;
	UCompanionComponent* GetCompanionComponent() const;

	/** Without a montage the ability ends itself on the next tick, after its cost and cooldown are paid. */
	void EndInstantUse();

	bool bSummonThisUse = true;
	bool bActed = false;
	bool bInstant = false;
};
