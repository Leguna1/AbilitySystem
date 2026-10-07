#include "PassiveAbility.h"

#include "AbilityComponent.h"
#include "ActiveAbility.h"
#include "CombatantComponent.h"

void UPassiveAbility::ActivatePassive()
{
	UCombatantComponent* Target = GetEffectTarget();

	// Rank 0 = granted but locked.
	if (bPassiveActive || GetAbilityRank() <= 0 || !IsValid(Target))
	{
		return;
	}

	bPassiveActive = true;

	// The permanent entry (key None): this passive's own modifiers and tags.
	Target->ApplyEffect(this, NAME_None, Modifiers, GetAbilityRank(), GrantedTags, 0.0f);

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
	if (UCombatantComponent* Target = GetEffectTarget())
	{
		Target->RemoveEffectsFromSource(this);
	}
}
UCombatantComponent* UPassiveAbility::GetEffectTarget() const
{
	const UAbilityComponent* Component = GetAbilityComponent();
	return IsValid(Component) ? Component->GetCombatantComponent() : nullptr;
}
void UPassiveAbility::ApplyTimedEffect(const FName EntryKey, const TArray<FStatModifier>& EffectModifiers, const FGameplayTagContainer& EffectTags, const float Duration)
{
	UCombatantComponent* Target = GetEffectTarget();

	if (!bPassiveActive || EntryKey.IsNone() || !IsValid(Target))
	{
		return;
	}

	Target->ApplyEffect(this, EntryKey, EffectModifiers, GetAbilityRank(), EffectTags, Duration);
}

void UPassiveAbility::RemoveTimedEffect(const FName EntryKey)
{
	if (UCombatantComponent* Target = GetEffectTarget(); Target && !EntryKey.IsNone())
	{
		Target->RemoveEffect(this, EntryKey);
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
	const UCombatantComponent* Target = GetEffectTarget();
	return IsValid(Target) && Target->HasEffect(this, EntryKey);
}
void UPassiveAbility::SetTimedEffectStacks(const FName EntryKey, const int32 Stacks)
{
	if (UCombatantComponent* Target = GetEffectTarget(); Target && !EntryKey.IsNone())
	{
		Target->SetEffectStacks(this, EntryKey, Stacks);
	}
}
#if WITH_EDITOR
#include "AbilityValidation.h"

EDataValidationResult UPassiveAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	return CombineDataValidationResults(Result,
		AbilityValidation::ValidateModifiers(Modifiers, TEXT("Modifiers"), Context));
}
#endif