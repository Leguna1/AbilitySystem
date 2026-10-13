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

	/** Seconds, by rank. 0 = until its charges are spent. */
	UPROPERTY(EditAnywhere, Category = "Buff")
	FAbilityRankedFloat DurationByRank = FAbilityRankedFloat(5.0f);

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
