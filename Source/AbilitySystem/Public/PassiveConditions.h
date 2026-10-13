#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PassiveTypes.h"
#include "UObject/Object.h"
#include "PassiveConditions.generated.h"

class UReactiveAbility;

/**
 * "If" of a passive: checked after a trigger matched. ALL conditions must pass, in list order
 * (put cheap checks first; a failed condition stops the rest, so a Chance after a Cooldown only rolls when ready).
 * Stateless: anything remembered (last trigger time, trigger count) lives on the passive.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class ABILITYSYSTEM_API UPassiveCondition : public UObject
{
	GENERATED_BODY()

public:
	virtual bool IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const { return true; }
};

/** Random chance, by the passive's rank (0..1). */
UCLASS(meta = (DisplayName = "Chance"))
class ABILITYSYSTEM_API UPassiveCondition_Chance : public UPassiveCondition
{
	GENERATED_BODY()

public:
	virtual bool IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	/** 0.2 = 20%. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	FAbilityRankedFloat ChanceByRank = FAbilityRankedFloat(1.0f);
};

/** You must have (or must not have) certain tags, e.g. a stance or a buff. */
UCLASS(meta = (DisplayName = "Owner Has Tags"))
class ABILITYSYSTEM_API UPassiveCondition_OwnerTags : public UPassiveCondition
{
	GENERATED_BODY()

public:
	virtual bool IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	/** All of these are required. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTagContainer RequiredTags;

	/** Any of these blocks it. */
	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTagContainer BlockedTags;
};

/** The other actor (who you hit, who hit you) must have (or must not have) certain tags, e.g. Status.Marked. */
UCLASS(meta = (DisplayName = "Other Actor Has Tags"))
class ABILITYSYSTEM_API UPassiveCondition_OtherActorTags : public UPassiveCondition
{
	GENERATED_BODY()

public:
	virtual bool IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FGameplayTagContainer BlockedTags;
};

/** At most once every N seconds (an internal cooldown), by the passive's rank. */
UCLASS(meta = (DisplayName = "Internal Cooldown"))
class ABILITYSYSTEM_API UPassiveCondition_Cooldown : public UPassiveCondition
{
	GENERATED_BODY()

public:
	virtual bool IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Condition")
	FAbilityRankedFloat SecondsByRank = FAbilityRankedFloat(10.0f);
};

/** Fires at most N times. The count resets whenever the passive turns on (learned, rank changed). 1 = once. */
UCLASS(meta = (DisplayName = "Max Triggers"))
class ABILITYSYSTEM_API UPassiveCondition_MaxTriggers : public UPassiveCondition
{
	GENERATED_BODY()

public:
	virtual bool IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "1"))
	int32 MaxTriggers = 1;
};
