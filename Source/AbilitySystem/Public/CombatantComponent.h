#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PayloadReceiver.h"
#include "ModifierTypes.h"
#include "HitReactionTypes.h"
#include "CombatantComponent.generated.h"

class UAbilityComponent;
class UResourceComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCombatantPayloadReceivedSignature, const FAbilityPayload&, Payload, float, DamageApplied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCombatantDamageDealtSignature, AActor*, Target, const FAbilityPayload&, Payload, float, DamageApplied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatantDiedSignature, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatantKilledSignature, AActor*, Victim);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatantRevivedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatantStatusesChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatantHitReactionSignature, const FHitReactionResult&, Result);

/**
 * Makes an actor a combat participant: receives payloads, applies damage to
 * its UResourceComponent, handles death, and reports combat events for both
 * sides of a hit (taken on the victim, dealt/killed on the instigator).
 *
 * Works with or without a UAbilityComponent, so the player and simple
 * enemies share it.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UCombatantComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatantComponent();

	/**
	 * The one way to hit something. An actor's own IPayloadReceiver
	 * implementation wins (custom targets: shields, destructibles); otherwise
	 * its combatant component receives. Returns whether the payload was accepted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	static bool DeliverPayload(AActor* Target, const FAbilityPayload& Payload);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool ReceivePayload(const FAbilityPayload& Payload);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool CanReceivePayload(const FAbilityPayload& Payload) const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsDead() const;

	/** Invulnerable via SetInvulnerable, or via State.Invulnerable on the ability component. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInvulnerable() const;

	/** For owners without an ability component (simple enemies). */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetInvulnerable(bool bNewInvulnerable) { bInvulnerable = bNewInvulnerable; }

	/** This owner accepted a payload. DamageApplied is what health actually lost. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FCombatantPayloadReceivedSignature OnPayloadReceived;

	/** This owner's payload was accepted by Target. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FCombatantDamageDealtSignature OnDamageDealt;

	/** This owner died. Killer may be null (non-payload deaths). */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FCombatantDiedSignature OnDied;

	/** This owner killed Victim. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FCombatantKilledSignature OnKilled;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FCombatantRevivedSignature OnRevived;
	
	/** Applies or refreshes a status. Keyed by status tag + source, so re-applying refreshes. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Status")
	void ApplyStatus(const FStatusApplication& Status, const UObject* Source);

	UFUNCTION(BlueprintPure, Category = "Combat|Status")
	bool HasStatusTag(FGameplayTag Tag) const;

	/** Tags granted by active statuses. */
	UFUNCTION(BlueprintPure, Category = "Combat|Status")
	FGameplayTagContainer GetStatusTags() const;

	/** Statuses were applied, refreshed, expired or cleared. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FCombatantStatusesChangedSignature OnStatusesChanged;
	
	/** Every active status with its remaining time, for debugging and target UI. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Status")
	void GetActiveStatuses(TArray<FActiveEffectInfo>& OutStatuses) const;
	
	/**
 * Walk speed before buffs and statuses. Enemy logic that switches speeds
 * (patrol, chase) should set this instead of MaxWalkSpeed, so slows still apply.
 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Movement")
	void SetBaseMaxWalkSpeed(float NewBaseSpeed);
	
	/** Current poise, including any recovery since the last poise damage. */
	UFUNCTION(BlueprintPure, Category = "Combat|Poise")
	float GetPoise() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Poise")
	float GetMaxPoise() const { return MaxPoise; }

	/** True while a recent break prevents another one. */
	UFUNCTION(BlueprintPure, Category = "Combat|Poise")
	bool IsStaggerImmune() const;

	/** The owner's ability-component tags plus its status tags. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool HasCombatTag(FGameplayTag Tag) const;

	/** A hit produced a reaction (flinch or stronger). Step 2 plays it; for now, Blueprints can listen. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
	FCombatantHitReactionSignature OnHitReaction;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Scales incoming damage. Hook for armor, difficulty, or future passives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float DamageTakenMultiplier = 1.0f;
	
	/** Stability. Hits drain it; when it runs out, the hit breaks through. 0 = every hit breaks through. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float MaxPoise = 100.0f;

	/** Seconds without poise damage before poise starts recovering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float PoiseRegenDelay = 2.0f;

	/** Poise recovered per second once recovering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float PoiseRegenRate = 50.0f;

	/** After a break, seconds during which poise can't break again (hits only flinch). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
	float StaggerImmunityDuration = 1.5f;

private:
	UFUNCTION()
	void HandleOwnerDeath();

	UFUNCTION()
	void HandleOwnerRevived();

	static UCombatantComponent* FindCombatant(const AActor* Actor);

	UPROPERTY(Transient)
	TObjectPtr<UResourceComponent> ResourceComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	/** Set only while a payload is applying damage, so a lethal hit can credit its killer. */
	TWeakObjectPtr<AActor> PendingInstigator;

	bool bInvulnerable = false;
	
	UFUNCTION()
	void HandleStatusExpiry();

	void HandleStatusesChanged();
	void ScheduleStatusExpiry();
	double GetWorldTime() const;

	UPROPERTY(Transient)
	FStatModifierContainer StatusContainer;

	FTimerHandle StatusExpiryTimer;
	
	UFUNCTION()
	void HandleOwnerModifiersChanged();

	/** Base walk speed x the owner's move-speed buffs x its move-speed statuses. */
	void ApplyMovementSpeed();

	/** Walk speed before modifiers, captured at BeginPlay. */
	float BaseMaxWalkSpeed = 0.0f;
	
	FHitReactionResult ResolveHitReaction(const FAbilityPayload& Payload);

	/** Poise as of LastPoiseDamageTime. Recovery after that is computed on demand. */
	float StoredPoise = 0.0f;

	double LastPoiseDamageTime = -1.0e9;
	double StaggerImmuneUntil = 0.0;
};