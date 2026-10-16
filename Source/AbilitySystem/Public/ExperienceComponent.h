#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ExperienceComponent.generated.h"

class UAttributeComponent;
class UCombatantComponent;
class UResourceComponent;
class UNiagaraSystem;
class USkillTreeComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FExperienceChangedSignature, int32, Experience, int32, ExperienceToNextLevel, int32, Level);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLevelUpSignature, int32, NewLevel);

/**
 * Experience and levels. Kills give experience (the victim's Experience Reward). Every level gained:
 *   - sets the Attribute Component's level (its Growth Per Level applies) and gives attribute points,
 *   - gives skill points to the Skill Tree Component,
 *   - refills Health and Focus (optional).
 *
 * Experience to go from level L to L+1 = Base Experience x L ^ Experience Growth.
 * With 100 and 1.5: 100, 283, 520, 800, 1118... so later levels take longer.
 */
UCLASS(ClassGroup = (Progression), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UExperienceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UExperienceComponent();

	/** Adds experience and levels up as many times as it covers. Negative amounts are ignored. */
	UFUNCTION(BlueprintCallable, Category = "Experience")
	void AddExperience(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Experience")
	int32 GetLevel() const { return Level; }

	/** Experience gathered toward the next level. */
	UFUNCTION(BlueprintPure, Category = "Experience")
	int32 GetExperience() const { return Experience; }

	/** Experience the current level needs in total. 0 at max level. */
	UFUNCTION(BlueprintPure, Category = "Experience")
	int32 GetExperienceToNextLevel() const;

	/** 0..1 toward the next level, for an XP bar. 1 at max level. */
	UFUNCTION(BlueprintPure, Category = "Experience")
	float GetLevelProgress() const;

	UFUNCTION(BlueprintPure, Category = "Experience")
	bool IsMaxLevel() const { return Level >= MaxLevel; }

	/** Experience needed to go from ForLevel to the next. */
	UFUNCTION(BlueprintPure, Category = "Experience")
	int32 GetExperienceRequiredForLevel(int32 ForLevel) const;

	/** Experience or level changed (also once at start, so UI can fill itself). */
	UPROPERTY(BlueprintAssignable, Category = "Experience")
	FExperienceChangedSignature OnExperienceChanged;

	/** A level was gained. Fires once per level, even when one kill gives several. */
	UPROPERTY(BlueprintAssignable, Category = "Experience")
	FLevelUpSignature OnLevelUp;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Experience for killing Victim. Default: an enemy's Experience Reward. Override for other rules (level difference...). */
	UFUNCTION(BlueprintNativeEvent, Category = "Experience")
	int32 GetExperienceForKill(AActor* Victim) const;
	virtual int32 GetExperienceForKill_Implementation(AActor* Victim) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience", meta = (ClampMin = "1"))
	int32 StartingLevel = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience", meta = (ClampMin = "1"))
	int32 MaxLevel = 30;

	/** Experience from level 1 to 2. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Curve", meta = (ClampMin = "1"))
	int32 BaseExperience = 100;

	/** How much faster each level's need grows. 1 = every level needs Base x Level (linear). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Curve", meta = (ClampMin = "0.0"))
	float ExperienceGrowth = 1.5f;

	/** Given to the Skill Tree Component per level gained. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Rewards", meta = (ClampMin = "0"))
	int32 SkillPointsPerLevel = 1;

	/** Given to the Attribute Component per level gained, to spend on core stats. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Rewards", meta = (ClampMin = "0"))
	int32 AttributePointsPerLevel = 5;

	/** Level up fills Health and Focus: one more kill to level can save the fight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Rewards")
	bool bRefillResourcesOnLevelUp = true;

	/** Played on the owner when a level is gained (once, even when one kill gives several levels). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Level Up")
	TObjectPtr<UNiagaraSystem> LevelUpEffect;

	/** Mesh socket the effect follows. None = the character's feet (mesh origin). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Level Up")
	FName LevelUpEffectSocket;

	/** Played as a 2D sound: the player always hears their own level up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Experience|Level Up")
	TObjectPtr<USoundBase> LevelUpSound;

private:
	void PlayLevelUpFeedback() const;

	UFUNCTION()
	void HandleKilled(AActor* Victim);

	void BroadcastExperienceChanged();

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;

	UPROPERTY(Transient)
	TObjectPtr<USkillTreeComponent> SkillTreeComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAttributeComponent> AttributeComponent;

	UPROPERTY(Transient)
	TObjectPtr<UResourceComponent> ResourceComponent;

	int32 Level = 1;
	int32 Experience = 0;
};
