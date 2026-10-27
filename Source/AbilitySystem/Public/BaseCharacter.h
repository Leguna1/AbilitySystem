#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameplayTagContainer.h"
#include "GenericTeamAgentInterface.h"
#include "TargetableInterface.h"
#include "BaseCharacter.generated.h"

class UAbilityComponent;
class UActiveAbility;
class UAnimMontage;
class UAudioComponent;
class USoundBase;
struct FAbilityPayload;
class UAttributeComponent;
class UCombatantComponent;
class UResourceComponent;
class UTexture2D;

/**
 * Parent of the player and every enemy: a team, where it sees from, where it is
 * aimed at, death (montage + sound) and a voice (effort and hurt sounds). Creates no components: they're added by the child
 * (C++ or Blueprint) and found here, so existing Blueprints reparent without losing settings.
 */
UCLASS(Abstract)
class ABILITYSYSTEM_API ABaseCharacter : public ACharacter, public IGenericTeamAgentInterface, public ITargetableInterface
{
	GENERATED_BODY()

public:
	virtual void PostInitializeComponents() override;

	/* -------------------- Team -------------------- */

	virtual FGenericTeamId GetGenericTeamId() const override { return FGenericTeamId(TeamId); }

	/* -------------------- Senses and aiming -------------------- */

	/** Seen from the eye socket, with a level view: perception's cone follows the body, not the head's pitch. */
	virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

	virtual FVector GetTargetAimLocation_Implementation() const override;

	/* -------------------- State -------------------- */

	UFUNCTION(BlueprintPure, Category = "Character")
	bool IsDead() const;

	/* -------------------- Components -------------------- */

	UFUNCTION(BlueprintPure, Category = "Character|Components")
	UAbilityComponent* GetAbilityComponent() const { return AbilityComponent; }

	UFUNCTION(BlueprintPure, Category = "Character|Components")
	UCombatantComponent* GetCombatantComponent() const { return CombatantComponent; }

	UFUNCTION(BlueprintPure, Category = "Character|Components")
	UResourceComponent* GetResourceComponent() const { return ResourceComponent; }

	UFUNCTION(BlueprintPure, Category = "Character|Components")
	UAttributeComponent* GetAttributeComponent() const { return AttributeComponent; }
	
	/** Gap between this character's capsule and Other's (center distance minus both radii). 0 or less = touching. */
	UFUNCTION(BlueprintPure, Category = "Character")
	float GetEdgeDistanceTo(const AActor* Other) const;
	
	/** Name shown in UI (player bar, pet panel, nameplates). */
	UFUNCTION(BlueprintPure, Category = "Character|Info")
	FText GetCharacterDisplayName() const { return CharacterDisplayName; }

	/** Portrait shown in UI (the pet panel's ring, a party frame). */
	UFUNCTION(BlueprintPure, Category = "Character|Info")
	UTexture2D* GetCharacterPortrait() const { return CharacterPortrait; }


protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Children override for their own death handling (AI stop, corpse); call Super to reach OnCharacterDied. */
	virtual void HandleCharacterDied(AActor* Killer);

	/** Extras after the death montage and sound: ragdoll, loot, a respawn screen for the player. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character")
	void OnCharacterDied(AActor* Killer);

	/**
	 * Picks the death montage. Default: a random one from Death Montages.
	 * Override for directional deaths (by Killer's side) or per-damage-type animations.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Character|Death")
	UAnimMontage* SelectDeathMontage(AActor* Killer) const;
	virtual UAnimMontage* SelectDeathMontage_Implementation(AActor* Killer) const;

	/* -------------------- Death -------------------- */

	/**
	 * One is picked at random. Turn off Enable Auto Blend Out in each montage,
	 * otherwise the body stands back up when it ends.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Death")
	TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	/** Always plays, and cuts off any effort or hurt sound still going. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Death")
	TObjectPtr<USoundBase> DeathSound;

	/* -------------------- Voice -------------------- */

	/** Mesh socket the voice comes from. Missing = the actor's location. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice")
	FName VoiceSocketName = TEXT("head");

	/** "Hah!" when an ability commits. Use a Sound Cue or MetaSound with several takes for variety. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Effort")
	TObjectPtr<USoundBase> EffortSound;

	/** Chance per committed ability, once the cooldown is over. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Effort", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EffortSoundChance = 0.4f;

	/** After an effort sound, the next can't play for a random time in [Min, Max]. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Effort", meta = (ClampMin = "0.0", Units = "s"))
	float EffortCooldownMin = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Effort", meta = (ClampMin = "0.0", Units = "s"))
	float EffortCooldownMax = 4.0f;

	/** Abilities with any of these tags never make an effort sound (e.g. dodge, hit reaction). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Effort")
	FGameplayTagContainer EffortExcludedAbilityTags;

	/** "Ugh!" when damage lands. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Hurt")
	TObjectPtr<USoundBase> HurtSound;

	/** Chance per damaging hit, once the cooldown is over. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Hurt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HurtSoundChance = 0.7f;

	/** After a hurt sound, the next can't play for a random time in [Min, Max]. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Hurt", meta = (ClampMin = "0.0", Units = "s"))
	float HurtCooldownMin = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Hurt", meta = (ClampMin = "0.0", Units = "s"))
	float HurtCooldownMax = 2.0f;

	/** Hits smaller than this fraction of Max Health stay silent. 0 = every hit can make a sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Voice|Hurt", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HurtMinDamageFraction = 0.02f;

	/** Plays Sound as this character's voice: replaces whatever voice line is still playing. */
	UFUNCTION(BlueprintCallable, Category = "Character|Voice")
	void PlayVoice(USoundBase* Sound);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** 0 = player, 1 = enemies. Different teams are hostile; the same team is friendly. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Team")
	uint8 TeamId = 0;

	/** Mesh socket perception sees from. Missing = the default eye height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Senses")
	FName EyeSocketName = TEXT("head");

	/** Mesh socket targeting and arrows aim at. Missing = the capsule's center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Senses")
	FName AimSocketName = TEXT("spine_03");
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Info")
	FText CharacterDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character|Info")
	TObjectPtr<UTexture2D> CharacterPortrait;


private:
	UFUNCTION()
	void HandleCombatantDied(AActor* Killer);

	UFUNCTION()
	void HandleAbilityCommitted(FGameplayTag AbilityId, UActiveAbility* Ability);

	UFUNCTION()
	void HandlePayloadReceivedForVoice(const FAbilityPayload& Payload, float DamageApplied);

	void PlayDeathFeedback(AActor* Killer);

	/** Rolls Chance against a cooldown that ends at NextAllowedTime; on success, sets a new random cooldown. */
	bool TryVoiceCooldown(double& NextAllowedTime, float Chance, float CooldownMin, float CooldownMax) const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceAudio;

	double NextEffortTime = 0.0;
	double NextHurtTime = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;

	UPROPERTY(Transient)
	TObjectPtr<UResourceComponent> ResourceComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAttributeComponent> AttributeComponent;
};