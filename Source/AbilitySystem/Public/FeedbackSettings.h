// FeedbackSettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FeedbackSettings.generated.h"

class UFeedbackSet;

/** Project Settings -> Game -> Feedback. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Feedback"))
class ABILITYSYSTEM_API UFeedbackSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/**
	 * Last parent of every Feedback Set: cues nothing else defines play from here.
	 * Put the world misses here (Cue.Projectile.Miss, Cue.Weapon.Miss with stone, wood, dirt variants).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Feedback")
	TSoftObjectPtr<UFeedbackSet> DefaultFeedbackSet;

	/** The same cue plays at most this many times per frame (a 30-arrow volley landing at once). 0 = no limit. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0"))
	int32 MaxSameCuePerFrame = 4;

	/** One-shot effects farther than this from every player camera are skipped (sounds still play). 0 = never skip. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance", meta = (ClampMin = "0.0", Units = "cm"))
	float EffectCullDistance = 8000.0f;
};
