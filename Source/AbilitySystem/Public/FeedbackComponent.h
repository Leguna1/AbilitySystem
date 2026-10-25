// FeedbackComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "FeedbackTypes.h"
#include "FeedbackComponent.generated.h"

class UAudioComponent;
class UMaterialInterface;
class UFeedbackSet;
class UNiagaraComponent;
class USceneComponent;

/** A loop that is running (or waiting for a set that defines it). */
USTRUCT()
struct FFeedbackLoop
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Cue;

	/** Starts minus stops. The loop ends when it reaches 0. */
	int32 RefCount = 0;

	TWeakObjectPtr<USceneComponent> AttachTo;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> Effect;

	UPROPERTY()
	TObjectPtr<UAudioComponent> Sound;

	bool bDetachWhenStopped = false;
	float FadeOutTime = 0.25f;

	/** Played where the loop was when it ends for good (not when the set changes). */
	FFeedbackEffect StopEffect;
	FName Socket;

	/** Niagara float parameters, re-applied whenever the loop respawns. */
	TMap<FName, float> NiagaraFloats;

	/** The overlay this loop put on its mesh, and what was there before (restored when it ends). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> AppliedOverlay;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PreviousOverlay;
};

/**
 * An actor's feedback: its Feedback Set, and the loops that follow it (weapon auras,
 * swing trails, projectile flight, status glows). One-shot cues go through the
 * Feedback Subsystem; this is a shortcut that plays them with this actor's set.
 *
 * Weapons and projectiles have one built in. Swap the look at runtime with
 * Set Override Set (a fire enchant), and running loops switch over.
 */
UCLASS(ClassGroup = (Feedback), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFeedbackComponent();

	/* -------------------- Set -------------------- */

	/** The override if one is set, else Feedback Set. Cues the override lacks come from Get Fallback Set. */
	UFUNCTION(BlueprintPure, Category = "Feedback")
	UFeedbackSet* GetActiveSet() const;

	/** Feedback Set while an override is on (what the override sits on), else null. */
	UFUNCTION(BlueprintPure, Category = "Feedback")
	UFeedbackSet* GetFallbackSet() const;

	/** Replaces the base set (projectiles do this per shot). Running loops switch over. */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void SetFeedbackSet(UFeedbackSet* NewSet);

	/**
	 * Temporarily wins over the base set (enchants, an ability's arrow look). Cues it doesn't
	 * define still come from the base set, so it only needs what differs. Null clears it.
	 * Running loops switch over.
	 */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void SetOverrideSet(UFeedbackSet* NewOverride);

	/* -------------------- One-shot -------------------- */

	/** Plays Cue at the owner's location, facing its forward. */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	bool PlayCue(FGameplayTag Cue);

	UFUNCTION(BlueprintCallable, Category = "Feedback")
	bool PlayCueAt(FGameplayTag Cue, FVector Location, FRotator Rotation);

	bool PlayCueWithContext(const FGameplayTag& Cue, const FFeedbackCueContext& Context) const;

	/* -------------------- Loops -------------------- */

	/**
	 * Starts Cue attached to AttachTo (empty = the owner's root), at the cue's Attach Socket.
	 * Starting a running loop only adds a reference; it ends after as many stops.
	 * Returns false if no set defines it yet (it still starts if a later set does).
	 */
	UFUNCTION(BlueprintCallable, Category = "Feedback", meta = (AdvancedDisplay = "AttachTo"))
	bool StartLoop(FGameplayTag Cue, USceneComponent* AttachTo = nullptr);

	/** Removes one reference; fades the loop out when none remain. */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void StopLoop(FGameplayTag Cue);

	/** Ends the loop regardless of references. */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void ClearLoop(FGameplayTag Cue, bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void StopAllLoops(bool bImmediate = false);

	UFUNCTION(BlueprintPure, Category = "Feedback")
	bool IsLoopActive(FGameplayTag Cue) const;

	/** Sets a float user parameter on a running loop's Niagara system (a field's Radius). Kept across set swaps. */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void SetLoopFloat(FGameplayTag Cue, FName Parameter, float Value);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** What this actor looks and sounds like. Projectiles get theirs from their data asset each shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UFeedbackSet> FeedbackSet;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Feedback")
	TObjectPtr<UFeedbackSet> OverrideSet;

private:
	FFeedbackLoop* FindLoop(const FGameplayTag& Cue);
	const FFeedbackLoop* FindLoop(const FGameplayTag& Cue) const;

	/** Spawns the loop's effect and sound from the active set. False if nothing defines it. */
	bool SpawnLoopVisuals(FFeedbackLoop& Loop, bool bPlayStartEffect) const;
	void DespawnLoopVisuals(FFeedbackLoop& Loop, bool bImmediate, bool bPlayStopEffect) const;

	/** One-shot at the loop's attach point (its start and stop effects). */
	void PlayAtLoop(const FFeedbackLoop& Loop, const FFeedbackEffect& Effect) const;

	/** The active set changed: every running loop restarts with the new look. */
	void RefreshLoops();

	FFeedbackCueContext MakeOwnerContext() const;

	UPROPERTY(Transient)
	TArray<FFeedbackLoop> Loops;
};
