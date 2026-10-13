#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "HitReactionTypes.h"
#include "PassiveTypes.h"
#include "UObject/Object.h"
#include "PassiveTriggers.generated.h"

class UReactiveAbility;

/**
 * "When" of a passive: decides whether an event fires it. A passive fires when ANY of its triggers match.
 * Stateless: configuration only.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ABILITYSYSTEM_API UPassiveTrigger : public UObject
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const { return false; }
};

/** When one of your hits lands (the target accepted it). */
UCLASS(meta = (DisplayName = "On Damage Dealt"))
class ABILITYSYSTEM_API UPassiveTrigger_DamageDealt : public UPassiveTrigger
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	/** Only hits from abilities with any of these tags (e.g. Ability.Bow.BullsEye). Empty = every hit. */
	UPROPERTY(EditAnywhere, Category = "Trigger")
	FGameplayTagContainer SourceAbilityTags;
};

/** When you are hit (you accepted the hit). */
UCLASS(meta = (DisplayName = "On Hit Received"))
class ABILITYSYSTEM_API UPassiveTrigger_HitReceived : public UPassiveTrigger
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	/** Only hits from abilities with any of these tags. Empty = every hit. */
	UPROPERTY(EditAnywhere, Category = "Trigger")
	FGameplayTagContainer SourceAbilityTags;
};

/** When a hit makes you react (flinch, stagger, knockback...). */
UCLASS(meta = (DisplayName = "On Hit Reaction"))
class ABILITYSYSTEM_API UPassiveTrigger_HitReaction : public UPassiveTrigger
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	/** The reaction must be at least this strong. */
	UPROPERTY(EditAnywhere, Category = "Trigger")
	EHitReactionType MinimumReaction = EHitReactionType::Stagger;
};

/** When you kill something. */
UCLASS(meta = (DisplayName = "On Kill"))
class ABILITYSYSTEM_API UPassiveTrigger_Kill : public UPassiveTrigger
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;
};

/** When you die. */
UCLASS(meta = (DisplayName = "On Death"))
class ABILITYSYSTEM_API UPassiveTrigger_Death : public UPassiveTrigger
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;
};

UENUM(BlueprintType)
enum class EPassiveAbilityMoment : uint8
{
	Activated,
	Committed,
	Ended
};

/** When one of your abilities starts, commits (its point of no return) or ends. */
UCLASS(meta = (DisplayName = "On Ability"))
class ABILITYSYSTEM_API UPassiveTrigger_Ability : public UPassiveTrigger
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Trigger")
	EPassiveAbilityMoment Moment = EPassiveAbilityMoment::Committed;

	/** Only abilities with any of these tags. Empty = every ability. */
	UPROPERTY(EditAnywhere, Category = "Trigger")
	FGameplayTagContainer AbilityTags;

	/** Ended only: fire just when the ability finished normally, not when cancelled or interrupted. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (EditCondition = "Moment == EPassiveAbilityMoment::Ended", EditConditionHides))
	bool bOnlyWhenCompleted = true;
};

/** When an ability sends a gameplay event (e.g. Event.Combo.Finished). */
UCLASS(meta = (DisplayName = "On Gameplay Event"))
class ABILITYSYSTEM_API UPassiveTrigger_GameplayEvent : public UPassiveTrigger
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (Categories = "Event"))
	FGameplayTag EventTag;

	/** Off = child tags match too (Event.Combo matches Event.Combo.Finished). */
	UPROPERTY(EditAnywhere, Category = "Trigger")
	bool bExactMatch = true;
};

/** When your health drops below a fraction of max (fires once per crossing). */
UCLASS(meta = (DisplayName = "On Health Below"))
class ABILITYSYSTEM_API UPassiveTrigger_HealthBelow : public UPassiveTrigger
{
	GENERATED_BODY()

public:
	virtual bool Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	/** 0.3 = fires when health goes from 30% or more to below 30%. */
	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthFraction = 0.3f;
};
