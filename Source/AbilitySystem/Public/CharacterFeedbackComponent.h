// CharacterFeedbackComponent.h
#pragma once

#include "CoreMinimal.h"
#include "FeedbackComponent.h"
#include "PayloadReceiver.h"
#include "CharacterFeedbackComponent.generated.h"

class UCombatantComponent;

/**
 * A character's feedback. Put it on any character with a Combatant component and give it
 * a Feedback Set; it plays by itself:
 *
 *   Statuses        Status.Burning lands -> loop Cue.Status.Burning on the mesh until it ends
 *                   (falls back to Cue.Status: one shared look for every status). The cue's
 *                   Start / Stop Effect play when it lands and wears off.
 *   Cue.Character.Hurt        lost health to a hit (rate-limited)
 *   Cue.Character.PoiseBreak  a hit broke poise (plays instead of Hurt)
 *   Cue.Character.Death / .Revive
 *
 * Also the source of body-attack hits (fists, bites): Cue.Weapon.Hit / Miss from this set.
 * Voice lines stay on the character (Death Sound, Hurt Sound); this is for effects and non-voice sound.
 */
UCLASS(ClassGroup = (Feedback), meta = (BlueprintSpawnableComponent, DisplayName = "Character Feedback"))
class ABILITYSYSTEM_API UCharacterFeedbackComponent : public UFeedbackComponent
{
	GENERATED_BODY()

public:
	UCharacterFeedbackComponent();

	/** The mesh loops attach to (sockets on the cues are looked up here). Default: the character's mesh. */
	UFUNCTION(BlueprintPure, Category = "Feedback")
	USceneComponent* GetAttachTarget() const;

	/** Plays Cue at the character's body (mesh bounds center), e.g. from a Blueprint event. */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	bool PlayCueOnBody(FGameplayTag Cue);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Minimum seconds between two Hurt cues, so rapid fire doesn't flash every arrow. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback", meta = (ClampMin = "0.0", Units = "s"))
	float HurtCueInterval = 0.25f;

	/** Play status loops (Cue.Status.*). Off = this character shows no status visuals. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	bool bShowStatuses = true;

private:
	UFUNCTION()
	void HandleEffectsChanged();

	UFUNCTION()
	void HandlePayloadReceived(const FAbilityPayload& Payload, float DamageApplied);

	UFUNCTION()
	void HandlePoiseDamaged(float Poise, float MaxPoise, bool bBroken);

	UFUNCTION()
	void HandleDied(AActor* Killer);

	UFUNCTION()
	void HandleRevived();

	/** Starts and stops status loops to match the statuses on the character right now. */
	void SyncStatusLoops();

	/** Status.Burning -> Cue.Status.Burning if that tag exists, else Cue.Status. */
	static FGameplayTag GetCueForStatus(const FGameplayTag& StatusTag);

	FVector GetBodyLocation() const;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> Combatant;

	/** Status on the character -> the loop cue started for it. */
	TMap<FGameplayTag, FGameplayTag> StatusLoops;

	double LastHurtCueTime = -1.0e9;

	/** Frame of the last poise break, so that hit doesn't also play Hurt. */
	uint64 PoiseBreakFrame = 0;
};
