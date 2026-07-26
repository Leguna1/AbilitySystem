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