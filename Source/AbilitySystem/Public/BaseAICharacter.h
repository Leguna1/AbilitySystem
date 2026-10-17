#pragma once

#include "CoreMinimal.h"
#include "BaseCharacter.h"
#include "PayloadReceiver.h"
#include "BaseAICharacter.generated.h"

class UMotionWarpingComponent;
class UWidgetComponent;

/** One attack an AI character can choose: which ability, from what distance, how often. */
USTRUCT(BlueprintType)
struct FAIAttack
{
	GENERATED_BODY()

	/** Ability id of a granted ability (e.g. Ability.Golem.Punch, Ability.Wolf.Bite). */
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

	/** Needs a clear view of the target. Without one, the character moves in until it has it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bRequiresLineOfSight = true;

	/**
	 * Never picked on its own: only when commanded. For a companion this is where auto-cast starts:
	 * on = auto-cast off (Pounce waits for your command), and the player can toggle it on the companion bar.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bCommandOnly = false;
};

/**
 * Parent of every AI-driven fighter, hostile or friendly: creates the framework's components,
 * holds the attack list the AI picks from, the nameplate, and corpse handling.
 * Children: Base Enemy (home, leash, roam, rewards) and Companion Character (follows a leader).
 */
UCLASS(Abstract)
class ABILITYSYSTEM_API ABaseAICharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	ABaseAICharacter();

	UFUNCTION(BlueprintPure, Category = "AI")
	float GetAggroRadius() const { return AggroRadius; }

	float GetSightBackwardOffset() const { return SightBackwardOffset; }
	float GetProximityAwarenessRadius() const { return ProximityAwarenessRadius; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (TitleProperty = "AbilityTag"))
	TArray<FAIAttack> Attacks;

	/** Random pause after an attack finishes, before the next one. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0", Units = "s"))
	float AttackRecoveryMin = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0", Units = "s"))
	float AttackRecoveryMax = 0.9f;

	/** Must be facing the target within this angle to start an attack. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0", ClampMax = "180", Units = "deg"))
	float MaxAttackAngle = 35.0f;

	/** Health bar above the character. Set its Widget Class in the Blueprint. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> HealthBarComponent;

	/** Set by the AI controller: true while fighting. Shows the nameplate. */
	void SetInCombat(bool bNewInCombat);

	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool IsInCombat() const { return bInCombat; }

protected:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleCharacterDied(AActor* Killer) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAbilityComponent> AIAbilityComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCombatantComponent> AICombatantComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UResourceComponent> AIResourceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAttributeComponent> AIAttributeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

	/** Hostiles within this distance (and in view) are noticed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float AggroRadius = 1200.0f;

	/** Moves the view cone back behind the eyes so targets close to the body are seen. Larger characters need more. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float SightBackwardOffset = 0.0f;

	/** Hostiles this close are noticed regardless of facing or line of sight (felt, not seen). 0 = off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception", meta = (ClampMin = "0.0"))
	float ProximityAwarenessRadius = 300.0f;

	/** Seconds the body stays after death. 0 = stays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI", meta = (ClampMin = "0.0"))
	float CorpseLifetime = 5.0f;

	/** Seconds the nameplate stays visible after leaving combat. 0 = hide at once. */
	UPROPERTY(EditDefaultsOnly, Category = "AI|UI", meta = (ClampMin = "0"))
	float NameplateHideDelay = 2.0f;

	/** Nameplate always visible, in combat or not (a companion's). */
	UPROPERTY(EditDefaultsOnly, Category = "AI|UI")
	bool bAlwaysShowNameplate = false;

	/** For Blueprint extras: combat music, an alert sound, an aggro icon, and so on. */
	UFUNCTION(BlueprintImplementableEvent, Category = "AI|Combat")
	void OnCombatStateChanged(bool bNowInCombat);

private:
	UFUNCTION()
	void HandlePayloadReceived(const FAbilityPayload& Payload, float DamageApplied);

	/** Hands this character's components to every resource, poise and effects bar in the nameplate. */
	void InitializeNameplate();

	void SetNameplateVisible(bool bVisible);

	bool bInCombat = false;
	FTimerHandle NameplateHideTimer;
};
