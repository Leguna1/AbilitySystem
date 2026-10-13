#include "PassiveConditions.h"

#include "AbilityComponent.h"
#include "CombatantComponent.h"
#include "ReactiveAbility.h"

namespace
{
	bool CombatantHasAllTags(const UCombatantComponent& Combatant, const FGameplayTagContainer& Tags)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (!Combatant.HasCombatTag(Tag))
			{
				return false;
			}
		}
		return true;
	}

	bool CombatantHasAnyTag(const UCombatantComponent& Combatant, const FGameplayTagContainer& Tags)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (Combatant.HasCombatTag(Tag))
			{
				return true;
			}
		}
		return false;
	}
}

bool UPassiveCondition_Chance::IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	return FMath::FRand() < ChanceByRank.Get(Reactive.GetAbilityRank());
}

bool UPassiveCondition_OwnerTags::IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	const UAbilityComponent* Component = Reactive.GetAbilityComponent();

	return IsValid(Component) &&
		Component->HasAllOwnerTags(RequiredTags) &&
		!Component->HasAnyOwnerTags(BlockedTags);
}

bool UPassiveCondition_OtherActorTags::IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	const UCombatantComponent* OtherCombatant = IsValid(Event.OtherActor)
		? Event.OtherActor->FindComponentByClass<UCombatantComponent>()
		: nullptr;

	if (!IsValid(OtherCombatant))
	{
		// No combatant: only passes if nothing is required.
		return RequiredTags.IsEmpty();
	}

	return CombatantHasAllTags(*OtherCombatant, RequiredTags) &&
		!CombatantHasAnyTag(*OtherCombatant, BlockedTags);
}

bool UPassiveCondition_Cooldown::IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	const UWorld* World = Reactive.GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	return World->GetTimeSeconds() - Reactive.GetLastTriggerTime() >= SecondsByRank.Get(Reactive.GetAbilityRank());
}

bool UPassiveCondition_MaxTriggers::IsMet(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	return Reactive.GetTriggerCount() < MaxTriggers;
}
