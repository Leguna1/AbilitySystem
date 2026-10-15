#include "ReactiveAbility.h"

#include "ActiveAbility.h"
#include "PassiveConditions.h"
#include "PassiveEffects.h"
#include "PassiveTriggers.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

/* ============================== Lifecycle ============================== */

void UReactiveAbility::OnPassiveActivated_Implementation()
{
	LastTriggerTime = -1.0e9;
	TriggerCount = 0;
	ActiveCharges.Reset();
	ActiveStacks.Reset();

	Super::OnPassiveActivated_Implementation();
}

void UReactiveAbility::OnPassiveDeactivated_Implementation()
{
	ActiveCharges.Reset();
	ActiveStacks.Reset();

	Super::OnPassiveDeactivated_Implementation();
}

/* ============================== Reaction ============================== */

void UReactiveAbility::HandleEvent(const FPassiveEventContext& Event)
{
	if (!IsPassiveActive() || bHandlingEvent || Triggers.IsEmpty() || Effects.IsEmpty())
	{
		return;
	}

	// WHEN: any trigger.
	const bool bTriggered = Triggers.ContainsByPredicate([this, &Event](const UPassiveTrigger* Trigger)
	{
		return IsValid(Trigger) && Trigger->Matches(*this, Event);
	});

	if (!bTriggered)
	{
		return;
	}

	// IF: every condition, in order.
	for (const UPassiveCondition* Condition : Conditions)
	{
		if (IsValid(Condition) && !Condition->IsMet(*this, Event))
		{
			return;
		}
	}

	// DO: every effect, in order.
	TGuardValue<bool> HandlingGuard(bHandlingEvent, true);

	const UWorld* World = GetWorld();
	LastTriggerTime = IsValid(World) ? World->GetTimeSeconds() : 0.0;
	++TriggerCount;

	for (const UPassiveEffect* Effect : Effects)
	{
		if (IsValid(Effect))
		{
			Effect->Execute(*this, Event);
		}

		// An effect may have unlearned or deactivated this ability.
		if (!IsPassiveActive())
		{
			return;
		}
	}

	OnReactionTriggered(Event);
}

/* ============================== Event hooks ============================== */

void UReactiveAbility::OnDamageDealt_Implementation(AActor* Target, const FAbilityPayload& Payload, const float DamageApplied)
{
	Super::OnDamageDealt_Implementation(Target, Payload, DamageApplied);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::DamageDealt;
	Event.OtherActor = Target;
	Event.Payload = Payload;
	Event.Damage = DamageApplied;
	HandleEvent(Event);
}

void UReactiveAbility::OnPayloadReceived_Implementation(const FAbilityPayload& Payload, const float DamageApplied)
{
	Super::OnPayloadReceived_Implementation(Payload, DamageApplied);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::HitReceived;
	Event.OtherActor = Payload.Instigator;
	Event.Payload = Payload;
	Event.Damage = DamageApplied;
	HandleEvent(Event);
}

void UReactiveAbility::OnKilled_Implementation(AActor* Victim)
{
	Super::OnKilled_Implementation(Victim);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::Killed;
	Event.OtherActor = Victim;
	HandleEvent(Event);
}

void UReactiveAbility::OnDied_Implementation(AActor* Killer)
{
	Super::OnDied_Implementation(Killer);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::Died;
	Event.OtherActor = Killer;
	HandleEvent(Event);
}

void UReactiveAbility::OnHitReaction_Implementation(const FHitReactionResult& Reaction)
{
	Super::OnHitReaction_Implementation(Reaction);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::HitReaction;
	Event.OtherActor = Reaction.Instigator;
	Event.HitReaction = Reaction;
	HandleEvent(Event);
}

void UReactiveAbility::OnHealthChanged_Implementation(const float HealthBefore, const float HealthAfter)
{
	Super::OnHealthChanged_Implementation(HealthBefore, HealthAfter);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::HealthChanged;
	Event.HealthBefore = HealthBefore;
	Event.HealthAfter = HealthAfter;
	HandleEvent(Event);
}

void UReactiveAbility::OnAbilityActivated_Implementation(UActiveAbility* Ability)
{
	Super::OnAbilityActivated_Implementation(Ability);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::AbilityActivated;
	Event.Ability = Ability;
	HandleEvent(Event);
}

void UReactiveAbility::OnAbilityCommitted_Implementation(UActiveAbility* Ability)
{
	Super::OnAbilityCommitted_Implementation(Ability);

	// Spend charges before the reaction runs, so a reaction that re-grants charges on commit isn't spent at once.
	ConsumeEffectCharges(Ability);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::AbilityCommitted;
	Event.Ability = Ability;
	HandleEvent(Event);
}

void UReactiveAbility::OnAbilityEnded_Implementation(UActiveAbility* Ability, const EAbilityEndReason EndReason)
{
	Super::OnAbilityEnded_Implementation(Ability, EndReason);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::AbilityEnded;
	Event.Ability = Ability;
	Event.EndReason = EndReason;
	HandleEvent(Event);
}

void UReactiveAbility::OnGameplayEvent_Implementation(const FGameplayTag EventTag, UActiveAbility* Source)
{
	Super::OnGameplayEvent_Implementation(EventTag, Source);

	FPassiveEventContext Event;
	Event.Type = EPassiveEventType::GameplayEvent;
	Event.EventTag = EventTag;
	Event.Ability = Source;
	HandleEvent(Event);
}

/* ============================== Stacks ============================== */

int32 UReactiveAbility::AddEffectStack(const FName EntryKey, const int32 MaxStacks)
{
	FEffectStacks* Existing = ActiveStacks.FindByPredicate([EntryKey](const FEffectStacks& Entry)
	{
		return Entry.EntryKey == EntryKey;
	});

	FEffectStacks& StackEntry = Existing ? *Existing : ActiveStacks.AddDefaulted_GetRef();
	StackEntry.EntryKey = EntryKey;

	// Ran out (or was removed) since the last stack: begin again.
	if (!HasTimedEffect(EntryKey))
	{
		StackEntry.Count = 0;
	}

	StackEntry.Count = FMath::Min(StackEntry.Count + 1, FMath::Max(MaxStacks, 1));
	return StackEntry.Count;
}

/* ============================== Charges ============================== */

void UReactiveAbility::SetEffectCharges(const FName EntryKey, const int32 Charges, const FGameplayTagContainer& ConsumedByAbilityTags)
{
	if (EntryKey.IsNone() || Charges <= 0)
	{
		return;
	}

	FEffectCharges* Existing = ActiveCharges.FindByPredicate([EntryKey](const FEffectCharges& Entry)
	{
		return Entry.EntryKey == EntryKey;
	});

	FEffectCharges& ChargeEntry = Existing ? *Existing : ActiveCharges.AddDefaulted_GetRef();
	ChargeEntry.EntryKey = EntryKey;
	ChargeEntry.Remaining = Charges;
	ChargeEntry.ConsumedByAbilityTags = ConsumedByAbilityTags;

	SetTimedEffectStacks(EntryKey, Charges);
}

void UReactiveAbility::ConsumeEffectCharges(const UActiveAbility* Ability)
{
	if (ActiveCharges.IsEmpty() || !IsValid(Ability))
	{
		return;
	}

	// Copy: spending edits ActiveCharges.
	const TArray<FEffectCharges> Snapshot = ActiveCharges;

	for (const FEffectCharges& Entry : Snapshot)
	{
		const auto MatchesEntry = [&Entry](const FEffectCharges& Other) { return Other.EntryKey == Entry.EntryKey; };

		// The effect may have run out on its own since it was applied.
		if (!HasTimedEffect(Entry.EntryKey))
		{
			ActiveCharges.RemoveAll(MatchesEntry);
			continue;
		}

		if (!Ability->GetAbilityTags().HasAny(Entry.ConsumedByAbilityTags))
		{
			continue;
		}

		const int32 Remaining = Entry.Remaining - 1;

		if (Remaining <= 0)
		{
			ActiveCharges.RemoveAll(MatchesEntry);
			RemoveTimedEffect(Entry.EntryKey);
			continue;
		}

		if (FEffectCharges* Live = ActiveCharges.FindByPredicate(MatchesEntry))
		{
			Live->Remaining = Remaining;
		}

		SetTimedEffectStacks(Entry.EntryKey, Remaining);
	}
}

/* ============================== Validation ============================== */

#if WITH_EDITOR
EDataValidationResult UReactiveAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// Only the classes people author need checking, not the native base.
	if (HasAnyFlags(RF_ClassDefaultObject) && GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Native))
	{
		return Result;
	}

	const bool bHasTriggers = Triggers.ContainsByPredicate([](const UPassiveTrigger* Trigger) { return IsValid(Trigger); });
	const bool bHasEffects = Effects.ContainsByPredicate([](const UPassiveEffect* Effect) { return IsValid(Effect); });

	if (!bHasTriggers)
	{
		Context.AddError(NSLOCTEXT("ReactiveAbility", "NoTriggers",
			"No Triggers, so this never reacts. Add one (e.g. On Gameplay Event), or make it a plain Passive Ability."));
		Result = EDataValidationResult::Invalid;
	}

	if (!bHasEffects)
	{
		Context.AddWarning(NSLOCTEXT("ReactiveAbility", "NoEffects", "No Effects, so reacting does nothing."));
	}

	const auto WarnEmpty = [&Context](const int32 EmptyCount, const TCHAR* ListName)
	{
		if (EmptyCount > 0)
		{
			Context.AddWarning(FText::Format(NSLOCTEXT("ReactiveAbility", "EmptyEntry", "{0} has an empty entry. Pick a type or remove it."),
				FText::FromString(ListName)));
		}
	};

	WarnEmpty(Triggers.FilterByPredicate([](const UPassiveTrigger* Trigger) { return !IsValid(Trigger); }).Num(), TEXT("Triggers"));
	WarnEmpty(Conditions.FilterByPredicate([](const UPassiveCondition* Condition) { return !IsValid(Condition); }).Num(), TEXT("Conditions"));
	WarnEmpty(Effects.FilterByPredicate([](const UPassiveEffect* Effect) { return !IsValid(Effect); }).Num(), TEXT("Effects"));

	for (const UPassiveEffect* Effect : Effects)
	{
		if (IsValid(Effect))
		{
			Result = CombineDataValidationResults(Result, Effect->IsDataValid(Context));
		}
	}

	return Result;
}
#endif
