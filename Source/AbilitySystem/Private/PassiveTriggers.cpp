#include "PassiveTriggers.h"

#include "ActiveAbility.h"

bool UPassiveTrigger_DamageDealt::Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	return Event.Type == EPassiveEventType::DamageDealt &&
		(SourceAbilityTags.IsEmpty() || Event.Payload.SourceAbilityTags.HasAny(SourceAbilityTags));
}

bool UPassiveTrigger_HitReceived::Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	return Event.Type == EPassiveEventType::HitReceived &&
		(SourceAbilityTags.IsEmpty() || Event.Payload.SourceAbilityTags.HasAny(SourceAbilityTags));
}

bool UPassiveTrigger_HitReaction::Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	return Event.Type == EPassiveEventType::HitReaction &&
		Event.HitReaction.Reaction >= MinimumReaction;
}

bool UPassiveTrigger_Kill::Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	return Event.Type == EPassiveEventType::Killed;
}

bool UPassiveTrigger_Death::Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	return Event.Type == EPassiveEventType::Died;
}

bool UPassiveTrigger_Ability::Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	EPassiveEventType Expected = EPassiveEventType::AbilityCommitted;

	switch (Moment)
	{
	case EPassiveAbilityMoment::Activated:
		Expected = EPassiveEventType::AbilityActivated;
		break;
	case EPassiveAbilityMoment::Committed:
		Expected = EPassiveEventType::AbilityCommitted;
		break;
	case EPassiveAbilityMoment::Ended:
		Expected = EPassiveEventType::AbilityEnded;
		break;
	}

	if (Event.Type != Expected || !IsValid(Event.Ability))
	{
		return false;
	}

	if (Moment == EPassiveAbilityMoment::Ended && bOnlyWhenCompleted && Event.EndReason != EAbilityEndReason::Completed)
	{
		return false;
	}

	return AbilityTags.IsEmpty() || Event.Ability->GetAbilityTags().HasAny(AbilityTags);
}

bool UPassiveTrigger_GameplayEvent::Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	if (Event.Type != EPassiveEventType::GameplayEvent || !EventTag.IsValid())
	{
		return false;
	}

	return bExactMatch ? Event.EventTag.MatchesTagExact(EventTag) : Event.EventTag.MatchesTag(EventTag);
}

bool UPassiveTrigger_HealthBelow::Matches(const UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	return Event.Type == EPassiveEventType::HealthChanged &&
		Event.HealthBefore >= HealthFraction &&
		Event.HealthAfter < HealthFraction;
}
