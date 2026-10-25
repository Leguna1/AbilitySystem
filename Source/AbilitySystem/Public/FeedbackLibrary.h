// FeedbackLibrary.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FeedbackLibrary.generated.h"

class UFeedbackSet;

/** Feedback for things that have no Feedback Component: ability hazards, fields, one-off Blueprint moments. */
UCLASS()
class ABILITYSYSTEM_API UFeedbackLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Plays Cue from Set (then its parents, then the project default) at Location. Hit adds surface and normal. */
	UFUNCTION(BlueprintCallable, Category = "Feedback", meta = (WorldContext = "WorldContextObject", AutoCreateRefTerm = "Hit"))
	static bool PlayFeedbackCue(const UObject* WorldContextObject, const UFeedbackSet* Set, FGameplayTag Cue, FVector Location, FRotator Rotation, const FHitResult& Hit);
};
