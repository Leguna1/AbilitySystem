#include "PassiveEffects.h"

#include "AbilityComponent.h"
#include "ActiveAbility.h"
#include "CombatantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ReactiveAbility.h"
#include "ResourceComponent.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "AbilityValidation.h"
#include "Misc/DataValidation.h"
#endif

AActor* UPassiveEffect::ResolveTarget(const UReactiveAbility& Reactive, const FPassiveEventContext& Event, const EPassiveEffectTarget Target)
{
	return Target == EPassiveEffectTarget::Self
		? static_cast<AActor*>(Reactive.GetOwningCharacter())
		: Event.OtherActor.Get();
}

/* ============================== Buff Self ============================== */

void UPassiveEffect_BuffSelf::Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	const int32 Rank = Reactive.GetAbilityRank();

	// The effect's own object name keys the buff, so several buff effects on one passive stay separate
	// and the same one refreshes instead of stacking.
	const FName BuffKey = GetFName();
	const float Duration = FMath::Max(DurationByRank.Get(Rank), 0.0f);
	const int32 MaxStacks = FMath::Max(MaxStacksByRank.Get(Rank), 1);

	if (MaxStacks > 1)
	{
		// One more stack (or a fresh first one if the buff ran out); the modifiers count once per stack.
		const int32 Stacks = Reactive.AddEffectStack(BuffKey, MaxStacks);

		TArray<FStatModifier> StackedModifiers = Modifiers;

		for (FStatModifier& Modifier : StackedModifiers)
		{
			for (float& Magnitude : Modifier.Magnitude.Values)
			{
				Magnitude *= static_cast<float>(Stacks);
			}
		}

		Reactive.ApplyTimedEffect(BuffKey, StackedModifiers, GrantedTags, Duration);
		Reactive.SetTimedEffectStacks(BuffKey, Stacks);
		return;
	}

	Reactive.ApplyTimedEffect(BuffKey, Modifiers, GrantedTags, Duration);

	const int32 Charges = ChargesByRank.Get(Rank);

	if (Charges > 0 && !ConsumedByAbilityTags.IsEmpty())
	{
		Reactive.SetEffectCharges(BuffKey, Charges, ConsumedByAbilityTags);
	}
}

#if WITH_EDITOR
EDataValidationResult UPassiveEffect_BuffSelf::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(Context),
		AbilityValidation::ValidateModifiers(Modifiers, TEXT("Buff Self: Modifiers"), Context));

	const bool bHasCharges = ChargesByRank.Get(1) > 0;

	if (bHasCharges && ConsumedByAbilityTags.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("PassiveEffects", "ChargesNoTags",
			"Buff Self has charges but no Consumed By Ability Tags, so the charges are never spent."));
	}

	const bool bStacks = MaxStacksByRank.Get(1) > 1;

	if (bHasCharges && bStacks)
	{
		Context.AddWarning(NSLOCTEXT("PassiveEffects", "ChargesAndStacks",
			"Buff Self has both charges and Max Stacks above 1. Stacking ignores charges; use one or the other."));
	}

	if (bStacks && DurationByRank.Get(1) <= 0.0f)
	{
		Context.AddWarning(NSLOCTEXT("PassiveEffects", "StacksForever",
			"Buff Self stacks but has no duration, so the stacks never fall off."));
	}

	if (!bHasCharges && DurationByRank.Get(1) <= 0.0f)
	{
		Context.AddWarning(NSLOCTEXT("PassiveEffects", "Forever",
			"Buff Self has no duration and no charges: once fired it lasts until the passive is unlearned."));
	}

	return Result;
}
#endif

/* ============================== Apply Status ============================== */

void UPassiveEffect_ApplyStatus::Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	AActor* RecipientActor = ResolveTarget(Reactive, Event, ApplyTo);
	UCombatantComponent* RecipientCombatant = IsValid(RecipientActor)
		? RecipientActor->FindComponentByClass<UCombatantComponent>()
		: nullptr;

	const UAbilityComponent* OwnerAbilities = Reactive.GetAbilityComponent();

	if (!IsValid(RecipientCombatant) || !IsValid(OwnerAbilities))
	{
		return;
	}

	const int32 Rank = FMath::Max(Reactive.GetAbilityRank(), 1);

	for (const FStatusEffectSpec& Spec : Statuses)
	{
		if (!Spec.StatusTag.IsValid() || !OwnerAbilities->HasAllOwnerTags(Spec.RequiredOwnerTags))
		{
			continue;
		}

		FStatusApplication Status;
		Status.StatusTag = Spec.StatusTag;
		Status.Display = Spec.Display;
		Status.Modifiers = Spec.Modifiers;
		Status.GrantedTags = Spec.GrantedTags;
		Status.Rank = Rank;
		Status.Duration = FMath::Max(Spec.DurationByRank.Get(Rank), 0.0f);

		// Same source as the owner's abilities use, so a shared status tag replaces the ability's own version.
		RecipientCombatant->ApplyStatus(Status, Reactive.GetOwningCharacter());
	}
}

#if WITH_EDITOR
EDataValidationResult UPassiveEffect_ApplyStatus::IsDataValid(FDataValidationContext& Context) const
{
	return CombineDataValidationResults(Super::IsDataValid(Context),
		AbilityValidation::ValidateStatusSpecs(Statuses, TEXT("Apply Status: Statuses"), Context));
}
#endif

/* ============================== Activate Ability ============================== */

void UPassiveEffect_ActivateAbility::Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	if (UAbilityComponent* OwnerAbilities = Reactive.GetAbilityComponent(); IsValid(OwnerAbilities) && AbilityId.IsValid())
	{
		OwnerAbilities->TryActivateAbility(AbilityId);
	}
}

/* ============================== Reduce Cooldown ============================== */

void UPassiveEffect_ReduceCooldown::Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	UAbilityComponent* OwnerAbilities = Reactive.GetAbilityComponent();

	if (!IsValid(OwnerAbilities))
	{
		return;
	}

	const int32 Rank = Reactive.GetAbilityRank();
	const float Seconds = FMath::Max(ReductionSecondsByRank.Get(Rank), 0.0f);
	const float RemainingFraction = FMath::Clamp(RemainingReductionByRank.Get(Rank), 0.0f, 1.0f);

	if (!bTriggeringAbility)
	{
		OwnerAbilities->ReduceCooldowns(Abilities, Seconds, RemainingFraction, false);
		return;
	}

	// The ability behind the event: named directly for ability events, or the source of the hit for damage events.
	// Exact ids only, so a category tag on the hit (Ability.Type.Special) can't reach other abilities.
	FGameplayTagContainer TriggeringIds;

	if (IsValid(Event.Ability))
	{
		TriggeringIds.AddTag(Event.Ability->GetAbilityId());
	}
	else
	{
		TriggeringIds = Event.Payload.SourceAbilityTags;
	}

	OwnerAbilities->ReduceCooldowns(TriggeringIds, Seconds, RemainingFraction, true);
}

#if WITH_EDITOR
EDataValidationResult UPassiveEffect_ReduceCooldown::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!bTriggeringAbility && Abilities.IsEmpty())
	{
		Context.AddError(NSLOCTEXT("PassiveEffects", "ReduceNoAbilities",
			"Reduce Cooldown has no Abilities. Pick them, or tick Triggering Ability."));
		Result = EDataValidationResult::Invalid;
	}

	if (ReductionSecondsByRank.Get(1) <= 0.0f && RemainingReductionByRank.Get(1) <= 0.0f)
	{
		Context.AddWarning(NSLOCTEXT("PassiveEffects", "ReduceNothing",
			"Reduce Cooldown takes off 0 seconds and 0% of the remaining time, so it does nothing."));
	}

	return Result;
}
#endif

/* ============================== Spawn Effect ============================== */

void UPassiveEffect_SpawnEffect::Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	AActor* SpawnActor = ResolveTarget(Reactive, Event, SpawnOn);

	if (!IsValid(NiagaraSystem) || !IsValid(SpawnActor))
	{
		return;
	}

	const ACharacter* SpawnCharacter = Cast<ACharacter>(SpawnActor);
	USkeletalMeshComponent* CharacterMesh = IsValid(SpawnCharacter) ? SpawnCharacter->GetMesh() : nullptr;

	if (!SocketName.IsNone() && IsValid(CharacterMesh) && CharacterMesh->DoesSocketExist(SocketName))
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(NiagaraSystem, CharacterMesh, SocketName,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(SpawnActor, NiagaraSystem, SpawnActor->GetActorLocation(), SpawnActor->GetActorRotation());
}

/* ============================== Play Sound ============================== */

void UPassiveEffect_PlaySound::Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	const AActor* SoundActor = ResolveTarget(Reactive, Event, PlayAt);

	if (IsValid(Sound) && IsValid(SoundActor))
	{
		UGameplayStatics::PlaySoundAtLocation(SoundActor, Sound, SoundActor->GetActorLocation());
	}
}

/* ============================== Send Gameplay Event ============================== */

void UPassiveEffect_SendGameplayEvent::Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	if (UAbilityComponent* OwnerAbilities = Reactive.GetAbilityComponent(); IsValid(OwnerAbilities) && EventTag.IsValid())
	{
		OwnerAbilities->BroadcastGameplayEvent(EventTag, nullptr);
	}
}

void UPassiveEffect_RestoreResource::Execute(UReactiveAbility& Reactive, const FPassiveEventContext& Event) const
{
	AActor* Target = ResolveTarget(Reactive, Event, ApplyTo);
	UResourceComponent* Resources = IsValid(Target) ? Target->FindComponentByClass<UResourceComponent>() : nullptr;

	if (!IsValid(Resources) || Resources->IsDead())
	{
		return;
	}

	const int32 Rank = Reactive.GetAbilityRank();
	const float MaxValue = Resources->GetResourceValue(Resource, EResourceValueType::Max);

	const float Amount =
		AmountByRank.Get(Rank) +
		MaxValue * PercentOfMaxByRank.Get(Rank) +
		FMath::Max(Event.Damage, 0.0f) * PercentOfDamageByRank.Get(Rank);

	if (Amount > 0.0f)
	{
		Resources->ModifyResource(Resource, EResourceValueType::Current, Amount);
	}
}
