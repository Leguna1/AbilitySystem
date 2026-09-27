#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilityTypes.generated.h"

UENUM(BlueprintType)
enum class EAbilityStatus : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	Active UMETA(DisplayName = "Active"),
	Ending UMETA(DisplayName = "Ending")
};

UENUM(BlueprintType)
enum class EAbilityEndReason : uint8
{
	Completed UMETA(DisplayName = "Completed"),
	Cancelled UMETA(DisplayName = "Cancelled"),
	Interrupted UMETA(DisplayName = "Interrupted"),

	/**
	 * The ability ended because another ability was accepted through its
	 * currently open Transition window.
	 */
	Transitioned UMETA(DisplayName = "Transitioned"),

	/**
	 * The ability ended because another ability was accepted before the
	 * ability's early-cancellation window closed.
	 */
	EarlyCancelled UMETA(DisplayName = "Early Cancelled"),

	Failed UMETA(DisplayName = "Failed")
};
UENUM(BlueprintType)
enum class EAbilityCostTrigger : uint8
{
	/** The ability calls RequestCommit itself. */
	Manual UMETA(DisplayName = "Manual"),

	/** Paid once, as soon as activation has succeeded. */
	OnActivate UMETA(DisplayName = "On Activate"),

	/** Paid once, the first time the cost event fires. */
	OnAnimationEvent UMETA(DisplayName = "On Animation Event"),

	/** Paid every time the cost event fires (e.g. per shot). The first payment commits. */
	OnEveryAnimationEvent UMETA(DisplayName = "On Every Animation Event")
};

UENUM(BlueprintType)
enum class EAbilityCooldownTrigger : uint8
{
	/** Starts at the first payment. */
	OnCommit UMETA(DisplayName = "On Commit"),

	/** Starts when the ability ends, for any reason, if it committed. */
	OnAbilityEnd UMETA(DisplayName = "On Ability End")
};

/** A float that scales with ability rank. Rank 1 reads Values[0]; ranks past the end reuse the last entry. */
USTRUCT(BlueprintType)
struct FAbilityRankedFloat
{
	GENERATED_BODY()

	FAbilityRankedFloat() = default;
	explicit FAbilityRankedFloat(const float InitialValue) { Values.Add(InitialValue); }
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Rank")
	TArray<float> Values;

	float Get(const int32 Rank) const
	{
		return Values.IsEmpty()
			? 0.0f
			: Values[FMath::Clamp(Rank - 1, 0, Values.Num() - 1)];
	}
};

/** Integer counterpart of FAbilityRankedFloat (shot counts, arrow counts, charges). */
USTRUCT(BlueprintType)
struct FAbilityRankedInt
{
	GENERATED_BODY()

	FAbilityRankedInt() = default;
	explicit FAbilityRankedInt(const int32 InitialValue) { Values.Add(InitialValue); }
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Rank")
	TArray<int32> Values;

	int32 Get(const int32 Rank) const
	{
		return Values.IsEmpty()
			? 0
			: Values[FMath::Clamp(Rank - 1, 0, Values.Num() - 1)];
	}
};