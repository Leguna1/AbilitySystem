#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "PayloadReceiver.h"
#include "BaseEnemy.generated.h"

class UInputBufferComponent;
class UMotionWarpingComponent;
class UWidgetComponent;
/**
 * Parent of every enemy: creates the framework's components, sets team 1,
 * and adds aggro, leash, proximity awareness, a home location and corpse handling.
 * Children set the mesh, animations, attributes and ability kit.
 */

USTRUCT(BlueprintType)
struct FEnemyAttack
{
	GENERATED_BODY()

	/** Ability id of a granted ability (e.g. Ability.Golem.Punch). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (Categories = "Ability"))
	FGameplayTag AbilityTag;

	/** Gap between capsules (edge to edge) where this attack can be used. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0", Units = "cm"))
	float MinRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0", Units = "cm"))
	float MaxRange = 150.0f;

	/** Higher = picked more often among ready attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;
	
	/** Higher is tried first: the best ready attack wins. Equal priorities are picked by Weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	int32 Priority = 0;

	/** Off = never walks up for this attack; it's only used when the target is already in range (e.g. a caster's melee). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bChaseIntoRange = true;

	/** Needs a clear view of the target. Without one, the enemy moves in until it has it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bRequiresLineOfSight = true;
};

UCLASS(Abstract)
class ABILITYSYSTEM_API ABaseEnemy : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ABaseEnemy();

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetAggroRadius() const { return AggroRadius; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	float GetLeashRadius() const { return LeashRadius; }

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetHomeLocation() const { return HomeLocation; }

	float GetSightBackwardOffset() const { return SightBackwardOffset; }
	float GetProximityAwarenessRadius() const { return ProximityAwarenessRadius; }
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat")
	TArray<FEnemyAttack> Attacks;

	/** Random pause after an attack finishes, before the next one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0", Units = "s"))
	float AttackRecoveryMin = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0", Units = "s"))
	float AttackRecoveryMax = 0.9f;

	/** Must be facing the target within this angle to start an attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy|Combat", meta = (ClampMin = "0", ClampMax = "180", Units = "deg"))
	float MaxAttackAngle = 35.0f;
	
	/** Out of combat, wanders to random points this far from home. 0 = stands still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roam", meta = (ClampMin = "0", Units = "cm"))
	float RoamRadius = 600.0f;

	/** Random pause between roam walks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roam", meta = (ClampMin = "0", Units = "s"))
	float RoamWaitMin = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Roam", meta = (ClampMin = "0", Units = "s"))
	float RoamWaitMax = 8.0f;

	/** Health bar above the enemy. Set its Widget Class in the Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarComponent;
	
	
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleCharacterDied(AActor* Killer) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAbilityComponent> EnemyAbilityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCombatantComponent> EnemyCombatantComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UResourceComponent> EnemyResourceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAttributeComponent> EnemyAttributeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	/** Hostiles within this distance (and in view) are noticed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0"))
	float AggroRadius = 1200.0f;

	/** Chased farther than this from home, the enemy gives up and returns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0"))
	float LeashRadius = 2500.0f;

	/** Seconds the body stays after death. 0 = stays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "0.0"))
	float CorpseLifetime = 5.0f;

	/** Moves the view cone back behind the eyes so targets close to the body are seen. Larger enemies need more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0"))
	float SightBackwardOffset = 0.0f;

	/** Hostiles this close are noticed regardless of facing or line of sight (felt, not seen). 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy|Perception", meta = (ClampMin = "0.0"))
	float ProximityAwarenessRadius = 300.0f;
	
	

private:
	UFUNCTION()
	void HandlePayloadReceived(const FAbilityPayload& Payload, float DamageApplied);

	FVector HomeLocation = FVector::ZeroVector;
};