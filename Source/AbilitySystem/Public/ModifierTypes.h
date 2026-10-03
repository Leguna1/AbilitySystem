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
/** Authored on an ability: a status its hits apply to the target. */
USTRUCT(BlueprintType)
struct FStatusEffectSpec
{
	GENERATED_BODY()

	/** Identity of the status, always granted to the target while active. Re-applying refreshes it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status", meta = (Categories = "Status"))
	FGameplayTag StatusTag;

	/**
	 * Modifiers on the target, at the applying ability's rank. Their scope is
	 * matched against the ATTACKING ability's tags (empty = every attack).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	TArray<FStatModifier> Modifiers;

	/** Extra tags on the target while active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FGameplayTagContainer GrantedTags;

	/** Seconds, by the applying ability's rank. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FAbilityRankedFloat DurationByRank = FAbilityRankedFloat(5.0f);

	/** Applied only while the ATTACKER has all of these tags (lets traits switch statuses on). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status")
	FGameplayTagContainer RequiredOwnerTags;
};

/** A status resolved for one hit, carried in the payload. */
USTRUCT(BlueprintType)
struct FStatusApplication
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Status")
	FGameplayTag StatusTag;

	UPROPERTY(BlueprintReadWrite, Category = "Status")
	TArray<FStatModifier> Modifiers;

	UPROPERTY(BlueprintReadWrite, Category = "Status")
	FGameplayTagContainer GrantedTags;

	/** Rank the modifiers' magnitudes are read at. */
	UPROPERTY(BlueprintReadWrite, Category = "Status")
	int32 Rank = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Status")
	float Duration = 0.0f;
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
	
	/** Duration this entry was applied with (0 = until removed). For UI countdowns. */
	UPROPERTY()
	float Duration = 0.0f;

	/** Optional count for UI (charges, stacks). 0 = not shown. */
	UPROPERTY()
	int32 Stacks = 0;
};
class UTexture2D;
/** One active effect, resolved for display. */
USTRUCT(BlueprintType)
struct FActiveEffectInfo
{
	GENERATED_BODY()

	/** Whatever applied the effect, usually a passive ability. */
	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UObject> Source;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	FName EntryKey;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	FText Description;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	TObjectPtr<UTexture2D> Icon;

	/** Seconds left. Below 0 = lasts until removed (e.g. Primed waiting for its skills). */
	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	float RemainingTime = -1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	float Duration = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	int32 Stacks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Effect")
	FGameplayTagContainer GrantedTags;
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

	/** Adds or replaces the entry for Source + EntryKey. ExpiresAt <= 0 = until removed. Stacks survive a refresh. */
	void Apply(const UObject* Source, FName EntryKey, const TArray<FStatModifier>& InModifiers, int32 Rank, const FGameplayTagContainer& InGrantedTags, double ExpiresAt, float Duration);

	/** True if the stack count changed. */
	bool SetStacks(const UObject* Source, FName EntryKey, int32 Stacks);

	const TArray<FModifierEntry>& GetEntries() const { return Entries; }

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
	
	void Reset() { Entries.Reset(); }
	
	/**
 * Active entries resolved for display, in application order.
 * Unkeyed entries (a passive's permanent bonus) are skipped unless bIncludeUnkeyed.
 */
	void GetDisplayInfo(TArray<FActiveEffectInfo>& OutInfo, double Now, bool bIncludeUnkeyed) const;

private:
	int32 FindEntry(const FObjectKey& SourceKey, FName EntryKey) const;

	UPROPERTY()
	TArray<FModifierEntry> Entries;
};