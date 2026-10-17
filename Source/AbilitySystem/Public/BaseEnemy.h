#pragma once

#include "CoreMinimal.h"
#include "BaseAICharacter.h"
#include "BaseEnemy.generated.h"

/**
 * Parent of every enemy: team 1, a home it roams around and returns to, a leash, and a kill reward.
 * Components, attacks, nameplate and corpse handling come from Base AI Character.
 * Children set the mesh, animations, attributes and ability kit.
 */
UCLASS(Abstract)
class ABILITYSYSTEM_API ABaseEnemy : public ABaseAICharacter
{
	GENERATED_BODY()

public:
	ABaseEnemy();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetLeashRadius() const { return LeashRadius; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetHomeLocation() const { return HomeLocation; }

	/** Out of combat, wanders to random points this far from home. 0 = stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roam", meta = (ClampMin = "0", Units = "cm"))
	float RoamRadius = 600.0f;

	/** Random pause between roam walks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roam", meta = (ClampMin = "0", Units = "s"))
	float RoamWaitMin = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roam", meta = (ClampMin = "0", Units = "s"))
	float RoamWaitMax = 8.0f;

	/** Experience the killer earns. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Rewards")
	int32 GetExperienceReward() const { return ExperienceReward; }

protected:
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Rewards", meta = (ClampMin = "0"))
	int32 ExperienceReward = 10;

	/** Chased farther than this from home, the enemy gives up and returns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0"))
	float LeashRadius = 2500.0f;

private:
	FVector HomeLocation = FVector::ZeroVector;
};
