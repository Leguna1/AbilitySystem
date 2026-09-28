#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/ObjectKey.h"
#include "ModifierTypes.generated.h"

UENUM(BlueprintType)
enum class EModifierOperation : uint8
{
	/** Flat amount added to the base value. */
	Add UMETA(DisplayName = "Add"),

	/** Fraction of the base: 0.1 = +10%, -0.2 = -20%. Percents on one stat sum before applying. */
	Percent UMETA(DisplayName = "Percent")
};

/** An authored modifier: which stat, how, how much (by rank), where it applies, and when. */
USTRUCT(BlueprintType)
struct FStatModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier", meta = (Categories = "Stat"))
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	EModifierOperation Operation = EModifierOperation::Percent;

	/** Read at the source's rank when applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	FAbilityRankedFloat Magnitude = FAbilityRankedFloat(0.0f);

	/** Applies only to abilities with any of these tags (parent tags match children). Empty = everywhere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	FGameplayTagContainer AbilityScope;

	/** Applies only while the owner has all of these tags (states, procs, boosters). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	FGameplayTagContainer RequiredOwnerTags;
};

/** A modifier with its magnitude resolved for the rank it was applied at. */
USTRUCT()
struct FResolvedStatModifier
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Stat;

	UPROPERTY()
	EModifierOperation Operation = EModifierOperation::Percent;

	UPROPERTY()
	float Magnitude = 0.0f;

	UPROPERTY()
	FGameplayTagContainer AbilityScope;

	UPROPERTY()
	FGameplayTagContainer RequiredOwnerTags;
};

/** Everything one source applied under one entry key. Applied, refreshed and expired as a unit. */
USTRUCT()
struct FModifierEntry
{
	GENERATED_BODY()

	FObjectKey Source;

	UPROPERTY()
	FName EntryKey;

	UPROPERTY()
	TArray<FResolvedStatModifier> Modifiers;

	/** Owner tags granted while this entry is active. */
	UPROPERTY()
	FGameplayTagContainer GrantedTags;

	/** World time this entry expires. <= 0 = until removed. */
	UPROPERTY()
	double ExpiresAt = 0.0;

	bool IsExpired(const double Now) const { return ExpiresAt > 0.0 && Now >= ExpiresAt; }
};

/**
 * Active modifiers and granted tags, grouped by source + entry key.
 * Plain struct so any component can host one (abilities, combatants).
 * Queries ignore expired entries even before they are pruned.
 */
USTRUCT()
struct ABILITYSYSTEM_API FStatModifierContainer
{
	GENERATED_BODY()

	/** Adds or replaces the entry for Source + EntryKey. ExpiresAt <= 0 = until removed. */
	void Apply(const UObject* Source, FName EntryKey, const TArray<FStatModifier>& InModifiers, int32 Rank, const FGameplayTagContainer& InGrantedTags, double ExpiresAt);

	bool Remove(const UObject* Source, FName EntryKey);
	bool RemoveAll(const UObject* Source);
	bool Contains(const UObject* Source, FName EntryKey) const;

	/** (Base + all Adds) x (1 + all Percents), for modifiers whose scope and owner-tag conditions hold. */
	float Evaluate(FGameplayTag Stat, float BaseValue, const FGameplayTagContainer& AbilityTags, const FGameplayTagContainer& OwnerTags, double Now) const;

	void AppendGrantedTags(FGameplayTagContainer& OutTags, double Now) const;

	/** Removes expired entries. True if anything was removed. */
	bool PruneExpired(double Now);

	/** Earliest pending expiry, or 0 if nothing is timed. */
	double GetNextExpiryTime() const;

private:
	int32 FindEntry(const FObjectKey& SourceKey, FName EntryKey) const;

	UPROPERTY()
	TArray<FModifierEntry> Entries;
};