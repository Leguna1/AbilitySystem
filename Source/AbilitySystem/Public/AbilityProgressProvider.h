#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AbilityProgressProvider.generated.h"

/** The kind of progress an ability reports, which decides how the bar renders. */
UENUM(BlueprintType)
enum class EAbilityProgressKind : uint8
{
	/** No progress bar should be shown. */
	None UMETA(DisplayName = "None"),

	/** Fills 0 -> 100%. Text shown as a percentage. */
	Charge UMETA(DisplayName = "Charge"),

	/** Depletes 100% -> 0. Text shown as remaining / total. */
	Count UMETA(DisplayName = "Count")
};

/**
 * A single snapshot of an ability's progress, already expressed in the
 * direction the bar should display it.
 */
USTRUCT(BlueprintType)
struct FAbilityProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ability|Progress")
	EAbilityProgressKind Kind = EAbilityProgressKind::None;

	/** 0..1, in display direction. Charge fills up; Count depletes. */
	UPROPERTY(BlueprintReadOnly, Category = "Ability|Progress")
	float Normalized = 0.0f;

	/** Count: remaining units (e.g. shots left). Unused for Charge. */
	UPROPERTY(BlueprintReadOnly, Category = "Ability|Progress")
	int32 Current = 0;

	/** Count: total units (e.g. maximum shots). Unused for Charge. */
	UPROPERTY(BlueprintReadOnly, Category = "Ability|Progress")
	int32 Max = 0;
};

UINTERFACE(BlueprintType, MinimalAPI)
class UAbilityProgressProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by any ability that wants to drive the shared ability progress
 * bar. The widget reads this off the active ability and never needs to know
 * which concrete ability it is talking to.
 */
class ABILITYSYSTEM_API IAbilityProgressProvider
{
	GENERATED_BODY()

public:
	/** Returns the current progress snapshot. Return Kind == None to hide the bar. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ability|Progress")
	FAbilityProgress GetAbilityProgress() const;
	virtual FAbilityProgress GetAbilityProgress_Implementation() const { return FAbilityProgress(); }
};