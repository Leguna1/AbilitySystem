#include "AttributeComponent.h"

#include "CombatantComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#define LOCTEXT_NAMESPACE "AttributeComponent"

UAttributeComponent::UAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	CurrentValues.SetNum(Attributes.Num());

	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		const FAttributeDefinition& Definition = Attributes[Index];
		CurrentValues[Index] = FMath::Clamp(Definition.BaseValue, Definition.MinValue, FMath::Max(Definition.MaxValue, Definition.MinValue));
	}

	// Effects live on the combatant; attributes recalculate whenever they change.
	CombatantComponent = GetOwner()->FindComponentByClass<UCombatantComponent>();

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnEffectsChanged.AddDynamic(this, &UAttributeComponent::HandleEffectsChanged);
	}

	RecalculateAttributes();
	ApplyWalkSpeed();
}

void UAttributeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnEffectsChanged.RemoveDynamic(this, &UAttributeComponent::HandleEffectsChanged);
	}

	Super::EndPlay(EndPlayReason);
}

bool UAttributeComponent::HasAttribute(const FGameplayTag Attribute) const
{
	return FindAttributeIndex(Attribute) != INDEX_NONE;
}

float UAttributeComponent::GetAttributeValue(const FGameplayTag Attribute, const float Fallback) const
{
	const int32 Index = FindAttributeIndex(Attribute);

	if (Index == INDEX_NONE)
	{
		return Fallback;
	}

	// Before BeginPlay there are no current values yet: the clamped base is the honest answer.
	if (!CurrentValues.IsValidIndex(Index))
	{
		const FAttributeDefinition& Definition = Attributes[Index];
		return FMath::Clamp(Definition.BaseValue, Definition.MinValue, FMath::Max(Definition.MaxValue, Definition.MinValue));
	}

	return CurrentValues[Index];
}

float UAttributeComponent::GetAttributeBase(const FGameplayTag Attribute, const float Fallback) const
{
	const int32 Index = FindAttributeIndex(Attribute);
	return Index != INDEX_NONE ? Attributes[Index].BaseValue : Fallback;
}

void UAttributeComponent::SetAttributeBase(const FGameplayTag Attribute, const float NewBase)
{
	const int32 Index = FindAttributeIndex(Attribute);

	if (Index == INDEX_NONE)
	{
		return;
	}

	Attributes[Index].BaseValue = NewBase;
	RecalculateAttributes();
}

void UAttributeComponent::HandleEffectsChanged()
{
	RecalculateAttributes();
}

void UAttributeComponent::RecalculateAttributes()
{
	// Not started yet: BeginPlay calculates everything once it runs.
	if (CurrentValues.Num() != Attributes.Num())
	{
		return;
	}

	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		const FAttributeDefinition& Definition = Attributes[Index];

		// Attributes belong to the character, not an ability, so they are evaluated without ability tags.
		const float Modified = IsValid(CombatantComponent)
			? CombatantComponent->GetModifiedValue(Definition.Attribute, Definition.BaseValue, FGameplayTagContainer())
			: Definition.BaseValue;

		const float NewValue = FMath::Clamp(Modified, Definition.MinValue, FMath::Max(Definition.MaxValue, Definition.MinValue));

		if (FMath::IsNearlyEqual(NewValue, CurrentValues[Index]))
		{
			continue;
		}

		const float OldValue = CurrentValues[Index];
		CurrentValues[Index] = NewValue;

		if (Definition.Attribute == WalkSpeedAttribute)
		{
			ApplyWalkSpeed();
		}

		OnAttributeChanged.Broadcast(Definition.Attribute, OldValue, NewValue);
	}
}

void UAttributeComponent::ApplyWalkSpeed() const
{
	if (!WalkSpeedAttribute.IsValid() || !HasAttribute(WalkSpeedAttribute))
	{
		return;
	}

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(OwnerCharacter) ? OwnerCharacter->GetCharacterMovement() : nullptr;

	if (IsValid(Movement))
	{
		Movement->MaxWalkSpeed = GetAttributeValue(WalkSpeedAttribute);
	}
}

int32 UAttributeComponent::FindAttributeIndex(const FGameplayTag Attribute) const
{
	return Attribute.IsValid()
		? Attributes.IndexOfByPredicate([Attribute](const FAttributeDefinition& Definition) { return Definition.Attribute == Attribute; })
		: INDEX_NONE;
}

#if WITH_EDITOR

EDataValidationResult UAttributeComponent::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FGameplayTag> Seen;

	for (int32 Index = 0; Index < Attributes.Num(); ++Index)
	{
		const FAttributeDefinition& Definition = Attributes[Index];
		const FText Where = FText::Format(LOCTEXT("Where", "Attributes [{0}]"), Index);

		if (!Definition.Attribute.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("NoTag", "{0}: no Attribute tag is set."), Where));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		if (Seen.Contains(Definition.Attribute))
		{
			Context.AddError(FText::Format(LOCTEXT("Duplicate", "{0}: {1} is listed twice. Keep one."), Where, FText::FromName(Definition.Attribute.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}

		Seen.Add(Definition.Attribute);

		if (Definition.MinValue > Definition.MaxValue)
		{
			Context.AddError(FText::Format(LOCTEXT("MinMax", "{0}: Min Value is above Max Value."), Where));
			Result = EDataValidationResult::Invalid;
		}
		else if (Definition.BaseValue < Definition.MinValue || Definition.BaseValue > Definition.MaxValue)
		{
			Context.AddWarning(FText::Format(LOCTEXT("BaseOutside", "{0}: Base Value is outside [Min, Max] and will be clamped."), Where));
		}
	}

	if (WalkSpeedAttribute.IsValid() && !Seen.Contains(WalkSpeedAttribute))
	{
		Context.AddWarning(LOCTEXT("WalkMissing", "Walk Speed Attribute is set but not listed in Attributes, so walk speed is never driven."));
	}

	return Result;
}

#endif

#undef LOCTEXT_NAMESPACE