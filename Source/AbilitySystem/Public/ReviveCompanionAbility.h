#pragma once

#include "CoreMinimal.h"
#include "MontageAbility.h"
#include "ReviveCompanionAbility.generated.h"

class ACompanionCharacter;
class UCompanionComponent;

/**
 * Brings your fallen companion back where it fell, with part of its health. Only usable while its body
 * is still there (the companion's Corpse Lifetime) and within range. Ends the summon's respawn wait.
 *
 * A skill tree node like any other (Starting Rank 0 under Summon). Its own cooldown is normal (On Commit).
 * Montage optional (kneel, a healing gesture): without one it acts right away.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UReviveCompanionAbility : public UMontageAbility
{
	GENERATED_BODY()

public:
	UReviveCompanionAbility();

	virtual bool CanActivateAbility_Implementation() const override;
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityCommitted_Implementation() override;
	virtual void OnAnimationEvent_Implementation(FGameplayTag EventTag) override;

protected:
	/** Share of max health it comes back with, by rank: 0.5 = half. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Revive")
	FAbilityRankedFloat HealthFractionByRank = FAbilityRankedFloat(0.5f);

	/** You must be this close to the body. 0 = any distance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Revive", meta = (ClampMin = "0", Units = "cm"))
	float MaxRange = 1500.0f;

	/** Animation event at which it revives. Empty = when the ability commits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Revive")
	FGameplayTag ActEventTag;

	/** It came back. For Blueprint extras: a light pillar, a howl. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Ability|Revive")
	void OnCompanionRevived(ACompanionCharacter* Companion);

private:
	void Act();
	void EndInstantUse();
	UCompanionComponent* GetCompanionComponent() const;

	bool bActed = false;
	bool bInstant = false;
};
