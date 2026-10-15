#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "GameplayTagContainer.h"
#include "ModifierTypes.h"
#include "PassiveTypes.h"
#include "UObject/Object.h"
#include "PassiveEffects.generated.h"

class UReactiveAbility;
class UNiagaraSystem;
class USoundBase;

/** Who an effect acts on. */
UENUM(BlueprintType)
enum class EPassiveEffectTarget : uint8
{
	/** The other side of the event: the target you hit, the attacker who hit you, your victim. */
	OtherActor UMETA(DisplayName = "Other Actor"),

	/** The passive's owner. */
	Self UMETA(DisplayName = "Self")
};

/**
 * "Do" of a passive: runs when it fires, in list order. Stateless: configuration only.
 * Effects never play montages; to animate, use Activate Ability (the ability owns the montage).
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ABILITYSYSTEM_API UPassiveEffect : public UObject
{
	GENERATED_BODY()

public:
	virtual void Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const {}

protected:
	static AActor* ResolveTarget(const UReactiveAbility& Reactive, const FPassiveEventContext& Event, EPassiveEffectTarget Target);
};

/**
 * Gives you a buff: modifiers and tags for a while. Firing again refreshes it.
 * With charges, using abilities with the consuming tags spends them, and the buff ends when they run out (Primed).
 * With Max Stacks above 1, firing again adds a stack instead: the modifiers count once per stack (Overdrive).
 */
UCLASS(meta = (DisplayName = "Buff Self"))
class ABILITYSYSTEM_API UPassiveEffect_BuffSelf : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual void Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** Applied while the buff lasts, read at the passive's rank. */
	UPROPERTY(EditAnywhere, Category = "Buff")
	TArray<FStatModifier> Modifiers;

	/** Owner tags while the buff lasts (e.g. State.Buff.QuickHands), for UI and other rules. */
	UPROPERTY(EditAnywhere, Category = "Buff")
	FGameplayTagContainer GrantedTags;

	/** Seconds, by rank. 0 = until its charges are spent. Each new stack refreshes it. */
	UPROPERTY(EditAnywhere, Category = "Buff")
	FAbilityRankedFloat DurationByRank = FAbilityRankedFloat(5.0f);

	/**
	 * Most stacks at once, by rank. 1 = no stacking (firing again only refreshes).
	 * The modifiers are per stack: +5% attack speed at 4 stacks = +20%. All stacks end together.
	 */
	UPROPERTY(EditAnywhere, Category = "Buff|Stacks", meta = (ClampMin = "1"))
	FAbilityRankedInt MaxStacksByRank = FAbilityRankedInt(1);

	/** Uses before the buff ends, by rank. 0 = no charges (lasts its duration). */
	UPROPERTY(EditAnywhere, Category = "Buff|Charges")
	FAbilityRankedInt ChargesByRank = FAbilityRankedInt(0);

	/** Committing an ability with any of these tags spends one charge. */
	UPROPERTY(EditAnywhere, Category = "Buff|Charges")
	FGameplayTagContainer ConsumedByAbilityTags;
};

/** Applies statuses (slow, mark, vulnerability...) to the other actor or to yourself. Re-applying refreshes. */
UCLASS(meta = (DisplayName = "Apply Status"))
class ABILITYSYSTEM_API UPassiveEffect_ApplyStatus : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual void Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	UPROPERTY(EditAnywhere, Category = "Status")
	EPassiveEffectTarget ApplyTo = EPassiveEffectTarget::OtherActor;

	/** Magnitudes and durations are read at the passive's rank. */
	UPROPERTY(EditAnywhere, Category = "Status")
	TArray<FStatusEffectSpec> Statuses;
};

/** Starts one of your abilities (a counter-attack, a roar, a blink). It plays its own montage. */
UCLASS(meta = (DisplayName = "Activate Ability"))
class ABILITYSYSTEM_API UPassiveEffect_ActivateAbility : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual void Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	/** Must be granted to the owner. Normal activation rules apply (cooldown, cost, what's playing). */
	UPROPERTY(EditAnywhere, Category = "Ability", meta = (Categories = "Ability"))
	FGameplayTag AbilityId;
};

/** Spawns a Niagara effect on you or the other actor. Visual only. */
UCLASS(meta = (DisplayName = "Spawn Effect"))
class ABILITYSYSTEM_API UPassiveEffect_SpawnEffect : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual void Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Effect")
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	UPROPERTY(EditAnywhere, Category = "Effect")
	EPassiveEffectTarget SpawnOn = EPassiveEffectTarget::Self;

	/** Mesh socket to attach to (follows it). None or missing = spawned at the actor's location. */
	UPROPERTY(EditAnywhere, Category = "Effect")
	FName SocketName;
};

/** Plays a sound at you or the other actor. */
UCLASS(meta = (DisplayName = "Play Sound"))
class ABILITYSYSTEM_API UPassiveEffect_PlaySound : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual void Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, Category = "Sound")
	EPassiveEffectTarget PlayAt = EPassiveEffectTarget::Self;
};

/**
 * Shortens the remaining cooldown of your abilities: by seconds, by a share of what's left, or both.
 * Abilities matches ids and their parents: Ability.Bow covers every bow ability.
 */
UCLASS(meta = (DisplayName = "Reduce Cooldown"))
class ABILITYSYSTEM_API UPassiveEffect_ReduceCooldown : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual void Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	/** Whose cooldowns: ability ids, or parents of them (Ability.Bow). */
	UPROPERTY(EditAnywhere, Category = "Cooldown", meta = (Categories = "Ability"))
	FGameplayTagContainer Abilities;

	/** Use the ability that caused the event instead (e.g. the one that dealt the damage). Abilities is ignored. */
	UPROPERTY(EditAnywhere, Category = "Cooldown")
	bool bTriggeringAbility = false;

	/** Seconds taken off, by rank. */
	UPROPERTY(EditAnywhere, Category = "Cooldown", meta = (ClampMin = "0.0"))
	FAbilityRankedFloat ReductionSecondsByRank = FAbilityRankedFloat(0.0f);

	/** Share of the remaining cooldown taken off, by rank: 0.25 = a quarter, 1 = ready at once. Applied before the seconds. */
	UPROPERTY(EditAnywhere, Category = "Cooldown", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FAbilityRankedFloat RemainingReductionByRank = FAbilityRankedFloat(0.0f);
};

/** Sends a gameplay event to your other passives (chains: "on Fury, also..."). */
UCLASS(meta = (DisplayName = "Send Gameplay Event"))
class ABILITYSYSTEM_API UPassiveEffect_SendGameplayEvent : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual void Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Event", meta = (Categories = "Event"))
	FGameplayTag EventTag;
};
