#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "HitReactionTypes.h"
#include "PayloadReceiver.h"
#include "PassiveTypes.generated.h"

class UActiveAbility;

/** What happened, as seen by a passive. */
UENUM(BlueprintType)
enum class EPassiveEventType : uint8
{
	DamageDealt,
	HitReceived,
	Killed,
	Died,
	AbilityActivated,
	AbilityCommitted,
	AbilityEnded,
	GameplayEvent,
	HitReaction,
	HealthChanged
};

/**
 * Everything a passive's triggers, conditions and effects can know about the event
 * that is being evaluated. Fields that don't apply to the event type stay at their defaults.
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEM_API FPassiveEventContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	EPassiveEventType Type = EPassiveEventType::DamageDealt;

	/** The other side: the target you hit, the attacker who hit you, your victim or your killer. */
	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	TObjectPtr<AActor> OtherActor = nullptr;

	/** The ability involved: activated, committed or ended, or the one that sent a gameplay event. */
	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	TObjectPtr<UActiveAbility> Ability = nullptr;

	/** The hit, for Damage Dealt and Hit Received. */
	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	FAbilityPayload Payload;

	/** Health actually lost, for Damage Dealt and Hit Received. */
	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	float Damage = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	FGameplayTag EventTag;

	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	EAbilityEndReason EndReason = EAbilityEndReason::Completed;

	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	FHitReactionResult HitReaction;

	/** Health as a fraction of max (0..1), before and after the change. */
	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	float HealthBefore = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Passive")
	float HealthAfter = 1.0f;
};
