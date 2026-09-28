#pragma once

#include "CoreMinimal.h"
#include "AbilityTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
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

	/* -------------------- Identity -------------------- */

	UFUNCTION(BlueprintPure, Category = "Ability")
	FGameplayTag GetAbilityId() const { return AbilityId; }

	UFUNCTION(BlueprintPure, Category = "Ability|Tags")
	const FGameplayTagContainer& GetAbilityTags() const { return AbilityTags; }

	/* -------------------- Display -------------------- */

	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Ability|Display")
	FText GetDescription() const { return Description; }

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

	/* -------------------- Definition -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Display")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FGameplayTag AbilityId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Tags")
	FGameplayTagContainer AbilityTags;

	/** Rank before any learning. 0 = must be learned before use; 1 = usable from the start. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rank", meta = (ClampMin = "0"))
	int32 StartingRank = 1;

	/** Highest reachable rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Rank", meta = (ClampMin = "1"))
	int32 MaxRank = 1;
	
	/** BaseValue with every modifier on Stat that applies to this ability's tags. */
	UFUNCTION(BlueprintPure, Category = "Ability|Modifiers")
	float GetModifiedFloat(FGameplayTag Stat, float BaseValue) const;

	UFUNCTION(BlueprintPure, Category = "Ability|Modifiers")
	int32 GetModifiedInt(FGameplayTag Stat, int32 BaseValue) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	int32 AbilityRank = 0;
};