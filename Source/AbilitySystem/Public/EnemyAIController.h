#pragma once

#include "CoreMinimal.h"
#include "CombatAIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class ABaseEnemy;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;

/**
 * Runs an enemy. The fight itself comes from Combat AI Controller; this adds:
 * sight (AI Perception) and close-range awareness to find hostiles, forgetting an unseen target,
 * the leash that sends it home, and roaming around home when idle.
 */
UCLASS()
class ABILITYSYSTEM_API AEnemyAIController : public ACombatAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	/** True after leashing, until the enemy is back near home. No new targets meanwhile. */
	UFUNCTION(BlueprintPure, Category = "Enemy|Target")
	bool IsReturningHome() const { return bReturningHome; }

	/** Being hit by a hostile makes it the target, if there isn't one yet. Also used by packs to alert members. */
	virtual void NotifyAttackedBy(AActor* Attacker) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	virtual void EvaluateTarget() override;
	virtual bool ThinkOverride() override;
	virtual void ThinkWithoutTarget() override;
	virtual void OnTargetAcquired(AActor* NewTarget) override;
	virtual bool IsEngaged() const override { return HasTarget() && !bReturningHome; }
	virtual void DrawBehaviorDebug() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Perception")
	TObjectPtr<UAIPerceptionComponent> Perception;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	/** Seconds a target can stay out of sight before it's forgotten. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Target", meta = (ClampMin = "0.0", Units = "s"))
	float ForgetTargetAfter = 5.0f;

	/** Within this distance of home, a returning enemy may pick targets again. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Target", meta = (ClampMin = "0.0", Units = "cm"))
	float HomeReachedDistance = 200.0f;

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	AActor* FindNearestSeenTarget() const;
	AActor* FindNearestTargetInProximity() const;

	/** Out of combat, an enemy farther than this from home walks back. The roam area, at least HomeReachedDistance. */
	float GetHomeAreaRadius() const;

	void ThinkReturnHome();
	void ThinkIdle();
	void StartIdleWait();
	bool FindRoamPoint(FVector& OutPoint) const;

	/** Sets bReturningHome and keeps State.Invulnerable on the enemy in sync with it. */
	void SetReturningHome(bool bNewReturningHome);

	TWeakObjectPtr<ABaseEnemy> Enemy;

	double LastSeenTime = 0.0;
	bool bReturningHome = false;
	double NextRoamTime = 0.0;
};
