#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AttributeComponent.generated.h"

class UCombatantComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAttributeChangedSignature, FGameplayTag, Attribute, float, OldValue, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAttributePointsChangedSignature, int32, UnspentPoints);

/** One character attribute: a base value that effects modify, kept inside [Min, Max]. */
USTRUCT(BlueprintType)
struct FAttributeDefinition
{
	GENERATED_BODY()

	/** Which attribute, e.g. Stat.MoveSpeed. Effects on this stat change it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute", meta = (Categories = "Stat"))
	FGameplayTag Attribute;

	/** Value before any effects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	float BaseValue = 1.0f;

	/** The current value never drops below this, however many slows stack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	float MinValue = 0.0f;

	/** The current value never rises above this, however many buffs stack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	float MaxValue = 10.0f;
};

/** "Each point of this core stat adds Amount to Attribute." */
USTRUCT(BlueprintType)
struct FAttributeContribution
{
	GENERATED_BODY()

	/** The attribute that grows. A group (Stat.AttackPower) feeds both its Min and Max. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute", meta = (Categories = "Stat"))
	FGameplayTag Attribute;

	/** Added per point of the core stat (its current value, so core stat buffs count too). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	float AmountPerPoint = 1.0f;
};

/** A core stat the player spends points on (Strength, Dexterity...) and what each point gives. */
USTRUCT(BlueprintType)
struct FCoreStatDefinition
{
	GENERATED_BODY()

	/** Also list it in Attributes: its Base Value is the starting value, Max Value the cap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core Stat", meta = (Categories = "Stat.Core"))
	FGameplayTag CoreStat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core Stat")
	FText DisplayName;

	/** Short text for the character sheet tooltip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core Stat", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Core Stat", meta = (TitleProperty = "Attribute"))
	TArray<FAttributeContribution> Contributions;
};

/** "Every level after the first adds AmountPerLevel to Attribute." */
USTRUCT(BlueprintType)
struct FAttributeGrowth
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute", meta = (Categories = "Stat"))
	FGameplayTag Attribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	float AmountPerLevel = 1.0f;
};

/**
 * The character's own numbers: move speed, attack speed, attack and magic power, armor, magic resist.
 * Current value = base, modified by every effect on that attribute, clamped to [Min, Max].
 * Effects on the attribute's group apply too, after its own: Stat.AttackPower changes both
 * Stat.AttackPower.Min and Stat.AttackPower.Max.
 * The character's value uses only unscoped effects. An ability can ask for its own view with
 * Get Attribute Value For Ability, which adds effects scoped to it ("+20% attack speed for bow abilities").
 * Broadcasts OnAttributeChanged; nothing needs to poll. Can drive the character's walk speed.
 *
 * Progression, all tunable in class defaults:
 *   Value before effects = Base + Growth Per Level x (Level - 1) + spent points (core stats)
 *                          + every core stat's current value x its contribution to this attribute.
 *   Effects then apply on top, so "+10% Attack Power" also scales what Strength gave.
 */
UCLASS(ClassGroup = (Ability), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API UAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAttributeComponent();

	UFUNCTION(BlueprintPure, Category = "Attributes")
	bool HasAttribute(FGameplayTag Attribute) const;

	/** Current value, after effects and clamping. Fallback if this character has no such attribute. */
	UFUNCTION(BlueprintPure, Category = "Attributes")
	float GetAttributeValue(FGameplayTag Attribute, float Fallback = 0.0f) const;

	/**
	 * The attribute as one ability sees it: the character's effects plus those scoped to AbilityTags,
	 * clamped like the character's value. Abilities use it for attributes they read for themselves (Attack Speed).
	 */
	UFUNCTION(BlueprintPure, Category = "Attributes")
	float GetAttributeValueForAbility(FGameplayTag Attribute, const FGameplayTagContainer& AbilityTags, float Fallback = 0.0f) const;

	UFUNCTION(BlueprintPure, Category = "Attributes")
	float GetAttributeBase(FGameplayTag Attribute, float Fallback = 0.0f) const;

	/** Changes the value before effects, e.g. AI switching between patrol and chase speed. Effects still apply on top. */
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetAttributeBase(FGameplayTag Attribute, float NewBase);

	/** An attribute's current value changed. */
	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FAttributeChangedSignature OnAttributeChanged;

	/* -------------------- Level and core stats -------------------- */

	UFUNCTION(BlueprintPure, Category = "Attributes|Level")
	int32 GetCharacterLevel() const { return CharacterLevel; }

	/** Applies Growth Per Level. The Experience Component calls this; enemies can set Character Level directly. */
	UFUNCTION(BlueprintCallable, Category = "Attributes|Level")
	void SetCharacterLevel(int32 NewLevel);

	/** The core stats in the order designed, for building the character sheet. */
	UFUNCTION(BlueprintPure, Category = "Attributes|Core Stats")
	const TArray<FCoreStatDefinition>& GetCoreStats() const { return CoreStats; }

	UFUNCTION(BlueprintPure, Category = "Attributes|Core Stats")
	bool GetCoreStatDefinition(FGameplayTag CoreStat, FCoreStatDefinition& OutDefinition) const;

	UFUNCTION(BlueprintPure, Category = "Attributes|Core Stats")
	int32 GetUnspentAttributePoints() const { return UnspentAttributePoints; }

	/** Points the player has put into CoreStat (not counting its base value or buffs). */
	UFUNCTION(BlueprintPure, Category = "Attributes|Core Stats")
	int32 GetSpentPoints(FGameplayTag CoreStat) const;

	UFUNCTION(BlueprintCallable, Category = "Attributes|Core Stats")
	void AddAttributePoints(int32 Amount);

	/** True when Count points can go into CoreStat right now (enough points, below the stat's Max Value). */
	UFUNCTION(BlueprintPure, Category = "Attributes|Core Stats")
	bool CanSpendAttributePoints(FGameplayTag CoreStat, int32 Count = 1) const;

	UFUNCTION(BlueprintCallable, Category = "Attributes|Core Stats")
	bool SpendAttributePoints(FGameplayTag CoreStat, int32 Count = 1);

	/** Gives back every spent point (respec, and handy while tuning). */
	UFUNCTION(BlueprintCallable, Category = "Attributes|Core Stats")
	void ResetSpentPoints();

	/**
	 * How much Attribute would change if CoreStat got Count more points: for "+2 Attack Power" previews on hover.
	 * Includes the attribute's group (Strength -> Stat.AttackPower shows on Stat.AttackPower.Min), excludes effects.
	 */
	UFUNCTION(BlueprintPure, Category = "Attributes|Core Stats")
	float GetContributionPreview(FGameplayTag CoreStat, FGameplayTag Attribute, int32 Count = 1) const;

	/** Unspent points changed (gained on level up, spent, reset). */
	UPROPERTY(BlueprintAssignable, Category = "Attributes|Core Stats")
	FAttributePointsChangedSignature OnAttributePointsChanged;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * This character's attributes. Typically Stat.MoveSpeed and Stat.AttackSpeed, plus for combat:
	 * Stat.AttackPower.Min/Max, Stat.MagicPower.Min/Max, Stat.Armor, Stat.MagicResist.
	 * Raise Max Value for those: it defaults to 10.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (TitleProperty = "Attribute"))
	TArray<FAttributeDefinition> Attributes;

	/** Attribute written to the character movement's Max Walk Speed, usually Stat.MoveSpeed. Empty = leave movement alone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (Categories = "Stat"))
	FGameplayTag WalkSpeedAttribute;

	/** What every point in each core stat gives. Each core stat must also be listed in Attributes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes|Core Stats", meta = (TitleProperty = "CoreStat"))
	TArray<FCoreStatDefinition> CoreStats;

	/** Points to spend at the start, before any level up. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes|Core Stats", meta = (ClampMin = "0"))
	int32 StartingAttributePoints = 0;

	/** Flat gains per level, on top of what points give: e.g. +15 Stat.MaxHealth, +2 Stat.AttackPower. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes|Level", meta = (TitleProperty = "Attribute"))
	TArray<FAttributeGrowth> GrowthPerLevel;

	/** Starting level. An Experience Component on the same actor overrides it with its own level. Per instance for enemies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes|Level", meta = (ClampMin = "1"))
	int32 CharacterLevel = 1;

private:
	UFUNCTION()
	void HandleEffectsChanged();

	void RecalculateAttributes();

	/**
	 * Base value after the attribute's own effects, then its group's (e.g. Stat.AttackPower for .Min), before clamping.
	 * Empty AbilityTags = the character's value; otherwise effects scoped to those tags count too.
	 */
	float EvaluateAttribute(const FAttributeDefinition& Definition, const FGameplayTagContainer& AbilityTags) const;

	static float ClampToDefinition(const FAttributeDefinition& Definition, float Value);

	void ApplyWalkSpeed() const;
	int32 FindAttributeIndex(FGameplayTag Attribute) const;
	int32 FindCoreStatIndex(FGameplayTag CoreStat) const;

	/** Base + level growth + spent points + core stat contributions: the value effects then modify. */
	float GetProgressionBase(const FAttributeDefinition& Definition) const;

	/** Sum of what Contributions give Attribute (directly or through its group) per point. */
	static float GetContributionPerPoint(const FCoreStatDefinition& CoreStatDefinition, FGameplayTag Attribute);

	/** Recalculation order: core stats first, so derived attributes read their fresh values. */
	void BuildRecalculationOrder();

	TArray<int32> RecalculationOrder;

	/** Spent points, one per entry in CoreStats. */
	TArray<int32> SpentPoints;

	int32 UnspentAttributePoints = 0;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;

	/** Current values, one per entry in Attributes. */
	TArray<float> CurrentValues;
};