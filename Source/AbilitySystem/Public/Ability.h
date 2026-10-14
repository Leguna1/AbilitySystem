#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "AbilitySystemLog.h"
#include "Ability.generated.h"

class ACharacter;
class UAbilityComponent;
class UTexture2D;

/**
 * Anything that can be granted to a character: identity, display data, tags
 * and rank. Grants, ranks and the skill tree work with this type.
 *
 * Activated abilities derive from UActiveAbility; always-on abilities will
 * derive from UPassiveAbility.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class ABILITYSYSTEM_API UAbility : public UObject
{
	GENERATED_BODY()

public:
	/** Called once per instance, right after creation. */
	virtual void InitializeAbility(UAbilityComponent* InAbilityComponent, ACharacter* InOwningCharacter);

	virtual UWorld* GetWorld() const override;
	
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

	/* -------------------- Identity -------------------- */

	UFUNCTION(BlueprintPure, Category = "Ability")
	FGameplayTag GetAbilityId() const { return AbilityId; }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	const FGameplayTagContainer& GetAbilityTags() const { return AbilityTags; }

	/* -------------------- Display -------------------- */

	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	FText GetDisplayName() const { return DisplayName; }

	/** The description as authored, tokens unfilled. For a readable version use Get Description At Rank. */
	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	FText GetDescription() const { return Description; }

	/**
	 * The description with every {Token} filled in with Rank's values (see AbilityDescription.h for the tokens).
	 * CompareToRank > 0 wraps values that differ from that rank in <Changed>...</>, for a Rich Text Block
	 * with a "Changed" style (e.g. green numbers in the next-rank column). 0 = plain text.
	 */
	UFUNCTION(BlueprintPure, Category = "Ability|Display", meta = (AdvancedDisplay = "CompareToRank"))
	FText GetDescriptionAtRank(int32 Rank, int32 CompareToRank = 0) const;

	/** Get Description At Rank on a class's defaults: for UI that shows abilities nobody has yet (skill tree). */
	UFUNCTION(BlueprintPure, Category = "Ability|Display", meta = (AdvancedDisplay = "CompareToRank"))
	static FText GetClassDescriptionAtRank(TSubclassOf<UAbility> AbilityClass, int32 Rank, int32 CompareToRank = 0);

	/** Shows the description at every rank and the tokens it can use. A button in the ability's Class Defaults. */
	UFUNCTION(CallInEditor, Category = "Ability|Display")
	void PreviewDescription();

	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	UTexture2D* GetIcon() const { return Icon; }

	/* -------------------- Rank -------------------- */

	/** Rank this instance was created at. 0 = locked. Snapshotted, so upgrades apply to new instances. */
	UFUNCTION(BlueprintPure, Category = "Ability|Rank")
	int32 GetAbilityRank() const { return AbilityRank; }

	UFUNCTION(BlueprintPure, Category = "Ability|Rank")
	int32 GetStartingRank() const { return StartingRank; }

	UFUNCTION(BlueprintPure, Category = "Ability|Rank")
	int32 GetMaxRank() const { return FMath::Max(MaxRank, StartingRank); }

	/* -------------------- References -------------------- */

	UFUNCTION(BlueprintPure, Category = "Ability")
	UAbilityComponent* GetAbilityComponent() const { return AbilityComponent; }

	UFUNCTION(BlueprintPure, Category = "Ability")
	ACharacter* GetOwningCharacter() const { return OwningCharacter; }
	
	/** BaseValue with every modifier on Stat that applies to this ability's tags. */
	UFUNCTION(BlueprintPure, Category = "Ability|Modifiers")
	float GetModifiedFloat(FGameplayTag Stat, float BaseValue) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Modifiers")
	int32 GetModifiedInt(FGameplayTag Stat, int32 BaseValue) const;

protected:
	UFUNCTION(BlueprintPure, Category = "Ability|Rank")
	float GetRankedFloat(const FAbilityRankedFloat& Value) const { return Value.Get(AbilityRank); }

	UFUNCTION(BlueprintPure, Category = "Ability|Rank")
	int32 GetRankedInt(const FAbilityRankedInt& Value) const { return Value.Get(AbilityRank); }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool OwnerHasTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool OwnerHasAllTags(const FGameplayTagContainer& Tags) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	bool OwnerHasAnyTags(const FGameplayTagContainer& Tags) const;

	/**
	 * The values the description's {Tokens} are filled from, at Rank. Found automatically from authored
	 * values; override to add your own (call Super first).
	 */
	virtual void GetDescriptionTokens(int32 Rank, TMap<FString, FText>& OutTokens) const;

	/* -------------------- Definition -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display")
	FText DisplayName;

	/**
	 * One description for every rank: write {Tokens} where numbers go, e.g.
	 * "Fires {ProjectileCount} arrows, each dealing {DamagePercent} Attack Power."
	 * Click Preview Description to see each rank and every token available.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display")
	TObjectPtr<UTexture2D> Icon;

	/** Unique id of this ability, e.g. Ability.Bow.ChargedShot. Cooldowns, ranks and the skill tree key on it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FGameplayTag AbilityId;

	/**
	 * Tags describing this ability, used by modifier scopes, transitions and triggers
	 * (e.g. Ability.Weapon.Bow, Ability.Type.Special). The Ability Id is always included automatically.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Tags")
	FGameplayTagContainer AbilityTags;

	/** Rank before any learning. 0 = must be learned before use; 1 = usable from the start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rank", meta = (ClampMin = "0"))
	int32 StartingRank = 1;

	/** Highest reachable rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rank", meta = (ClampMin = "1"))
	int32 MaxRank = 1;
	
	

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	int32 AbilityRank = 0;
	
	/** Keeps AbilityId inside AbilityTags, so scopes and triggers can always match an ability by its id. */
	void EnsureIdInTags();

#if WITH_EDITORONLY_DATA
	FGameplayTag PreviousAbilityId;
#endif
};