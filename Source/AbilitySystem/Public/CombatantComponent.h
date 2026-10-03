#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PayloadReceiver.h"
#include "ModifierTypes.h"
#include "CombatantComponent.generated.h"

class UAbilityComponent;
class UResourceComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCombatantPayloadReceivedSignature, const FAbilityPayload&, Payload, float, DamageApplied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCombatantDamageDealtSignature, AActor*, Target, const FAbilityPayload&, Payload, float, DamageApplied);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatantDiedSignature, AActor*, Killer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCombatantKilledSignature, AActor*, Victim);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatantRevivedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatantStatusesChangedSignature);

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Scales incoming damage. Hook for armor, difficulty, or future passives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (ClampMin = "0.0"))
	float DamageTakenMultiplier = 1.0f;

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
};