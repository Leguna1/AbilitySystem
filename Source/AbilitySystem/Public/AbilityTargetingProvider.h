#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "AbilityTargetingProvider.generated.h"

class AActor;

USTRUCT(BlueprintType)
struct FAbilityTargetingRequest
{
	GENERATED_BODY()

	/** Identifies the requested targeting style or policy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FGameplayTag TargetingMode;

	/** Actor requesting targeting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	TObjectPtr<AActor> SourceActor = nullptr;

	/** Starting point for traces or targeting calculations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FVector Origin = FVector::ZeroVector;

	/** Desired targeting direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting", meta = (ClampMin = "0.0"))
	float MaximumRange = 10000.0f;
};

USTRUCT(BlueprintType)
struct FAbilityTargetingResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	FVector SurfaceNormal = FVector::UpVector;

	void Reset()
	{
		bValid = false;
		TargetActor = nullptr;
		TargetLocation = FVector::ZeroVector;
		SurfaceNormal = FVector::UpVector;
	}
};

UINTERFACE(BlueprintType)
class ABILITYSYSTEM_API UAbilityTargetingProvider : public UInterface
{
	GENERATED_BODY()
};

/**
 * Optional targeting service used by abilities.
 *
 * UAbilityComponent only forwards targeting requests. It does not implement
 * crosshair, melee, ground-placement or target-selection behavior itself.
 */
class ABILITYSYSTEM_API IAbilityTargetingProvider
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ability|Targeting")
	bool ResolveAbilityTargeting(const FAbilityTargetingRequest& Request, FAbilityTargetingResult& OutResult);
};