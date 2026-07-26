#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputBufferTypes.generated.h"

UENUM(BlueprintType)
enum class EBufferedInputSource : uint8
{
	Pressed UMETA(DisplayName = "Pressed"),
	Held UMETA(DisplayName = "Held")
};

/**
 * One input candidate returned by UInputBufferComponent.
 *
 * Pressed entries expire after their buffer duration.
 * Held entries are generated while an input remains held.
 */
USTRUCT(BlueprintType)
struct FBufferedInput
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input Buffer")
	FGameplayTag InputTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input Buffer")
	EBufferedInputSource Source = EBufferedInputSource::Pressed;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input Buffer")
	float BufferedAtTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input Buffer")
	float ExpiresAtTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input Buffer")
	int32 Sequence = 0;

	bool IsExpired(const float CurrentTime) const
	{
		return Source == EBufferedInputSource::Pressed && CurrentTime > ExpiresAtTime;
	}
};