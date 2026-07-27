#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BowDataAsset.generated.h"

class UNiagaraSystem;
class USoundBase;

UENUM(BlueprintType)
enum class EBowFeedbackPoint : uint8
{
	AbilityStart	UMETA(DisplayName = "Ability Start"),
	SpawnArrow		UMETA(DisplayName = "Spawn Arrow"),
	NockArrow		UMETA(DisplayName = "Nock Arrow"),
	ReleaseArrow	UMETA(DisplayName = "Release Arrow"),
	AbilityEnd		UMETA(DisplayName = "Ability End")
};

USTRUCT(BlueprintType)
struct FBowFeedbackSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UNiagaraSystem> Effect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback|Timing")
	EBowFeedbackPoint ActivateAt = EBowFeedbackPoint::AbilityStart;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Feedback|Timing")
	EBowFeedbackPoint ClearAt = EBowFeedbackPoint::AbilityEnd;
};

UCLASS(BlueprintType)
class ABILITYSYSTEM_API UBowDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Feedback")
	FBowFeedbackSet StartFeedback;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Feedback")
	FBowFeedbackSet OngoingFeedback;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Feedback")
	FBowFeedbackSet EndFeedback;
};