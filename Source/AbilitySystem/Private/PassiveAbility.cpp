#include "PassiveAbility.h"

#include "AbilityComponent.h"
#include "ActiveAbility.h"

void UPassiveAbility::ActivatePassive()
{
	UAbilityComponent* Component = GetAbilityComponent();

	// Rank 0 = granted but locked.
	if (bPassiveActive || GetAbilityRank() <= 0 || !IsValid(Component))
	{
		return;
	}

	bPassiveActive = true;

	// The permanent entry (key None): this passive's own modifiers and tags.
	Component->ApplyModifiers(this, NAME_None, Modifiers, GetAbilityRank(), GrantedTags, 0.0f);

	OnPassiveActivated();
}

void UPassiveAbility::DeactivatePassive()
{
	if (!bPassiveActive)
	{
		return;
	}

	OnPassiveDeactivated();

	bPassiveActive = false;

	// Removes the permanent entry and any running timed effects in one call.
	if (UAbilityComponent* Component = GetAbilityComponent())
	{
		Component->RemoveModifiersFromSource(this);
	}
}

void UPassiveAbility::ApplyTimedEffect(const FName EntryKey, const TArray<FStatModifier>& EffectModifiers, const FGameplayTagContainer& EffectTags, const float Duration)
{
	UAbilityComponent* Component = GetAbilityComponent();

	if (!bPassiveActive || EntryKey.IsNone() || !IsValid(Component))
	{
		return;
	}

	Component->ApplyModifiers(this, EntryKey, EffectModifiers, GetAbilityRank(), EffectTags, Duration);
}

void UPassiveAbility::RemoveTimedEffect(const FName EntryKey)
{
	UAbilityComponent* Component = GetAbilityComponent();

	if (!EntryKey.IsNone() && IsValid(Component))
	{
		Component->RemoveModifierEntry(this, EntryKey);
	}
}

void UPassiveAbility::OnPassiveActivated_Implementation()
{
}

void UPassiveAbility::OnPassiveDeactivated_Implementation()
{
}

void UPassiveAbility::OnDamageDealt_Implementation(AActor* Target, const FAbilityPayload& Payload, float DamageApplied)
{
}

void UPassiveAbility::OnPayloadReceived_Implementation(const FAbilityPayload& Payload, float DamageApplied)
{
}

void UPassiveAbility::OnKilled_Implementation(AActor* Victim)
{
}

void UPassiveAbility::OnDied_Implementation(AActor* Killer)
{
}

void UPassiveAbility::OnAbilityActivated_Implementation(UActiveAbility* Ability)
{
}

void UPassiveAbility::OnAbilityCommitted_Implementation(UActiveAbility* Ability)
{
}

void UPassiveAbility::OnAbilityEnded_Implementation(UActiveAbility* Ability, EAbilityEndReason EndReason)
{
}
void UPassiveAbility::OnGameplayEvent_Implementation(FGameplayTag EventTag, UActiveAbility* Source)
{
}

bool UPassiveAbility::HasTimedEffect(const FName EntryKey) const
{
	const UAbilityComponent* Component = GetAbilityComponent();
	return IsValid(Component) && Component->HasModifierEntry(this, EntryKey);
}
void UPassiveAbility::SetTimedEffectStacks(const FName EntryKey, const int32 Stacks)
{
	UAbilityComponent* Component = GetAbilityComponent();

	if (!EntryKey.IsNone() && IsValid(Component))
	{
		Component->SetModifierEntryStacks(this, EntryKey, Stacks);
	}
}