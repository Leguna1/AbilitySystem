#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AttributeComponent.generated.h"

class UCombatantComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAttributeChangedSignature, FGameplayTag, Attribute, float, OldValue, float, NewValue);

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

/**
 * The character's own numbers, such as move speed and attack speed.
 * Current value = base, modified by every effect on that attribute, clamped to [Min, Max].
 * Attributes belong to the character, so only unscoped effects apply.
 * Broadcasts OnAttributeChanged; nothing needs to poll. Can drive the character's walk speed.
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

	UFUNCTION(BlueprintPure, Category = "Attributes")
	float GetAttributeBase(FGameplayTag Attribute, float Fallback = 0.0f) const;

	/** Changes the value before effects, e.g. AI switching between patrol and chase speed. Effects still apply on top. */
	UFUNCTION(BlueprintCallable, Category = "Attributes")
	void SetAttributeBase(FGameplayTag Attribute, float NewBase);

	/** An attribute's current value changed. */
	UPROPERTY(BlueprintAssignable, Category = "Attributes")
	FAttributeChangedSignature OnAttributeChanged;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** This character's attributes. Typically Stat.MoveSpeed and Stat.AttackSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (TitleProperty = "Attribute"))
	TArray<FAttributeDefinition> Attributes;

	/** Attribute written to the character movement's Max Walk Speed, usually Stat.MoveSpeed. Empty = leave movement alone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attributes", meta = (Categories = "Stat"))
	FGameplayTag WalkSpeedAttribute;

private:
	UFUNCTION()
	void HandleEffectsChanged();

	void RecalculateAttributes();
	void ApplyWalkSpeed() const;
	int32 FindAttributeIndex(FGameplayTag Attribute) const;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;

	/** Current values, one per entry in Attributes. */
	TArray<float> CurrentValues;
};