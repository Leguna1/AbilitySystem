#include "CombatantComponent.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"
#include "ResourceComponent.h"
#include "AbilitySystemSettings.h"
#include "Engine/World.h"
#include "TimerManager.h"


UCombatantComponent::UCombatantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatantComponent::BeginPlay()
{
	Super::BeginPlay();
	
	StoredPoise = MaxPoise;

	ResourceComponent = GetOwner()->FindComponentByClass<UResourceComponent>();
	AbilityComponent = GetOwner()->FindComponentByClass<UAbilityComponent>();
	

	if (!IsValid(ResourceComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("UCombatantComponent on %s has no UResourceComponent; payloads will be accepted but deal no damage."),
			*GetNameSafe(GetOwner()));
		return;
	}

	ResourceComponent->OnDeath.AddDynamic(this, &UCombatantComponent::HandleOwnerDeath);
	ResourceComponent->OnRevived.AddDynamic(this, &UCombatantComponent::HandleOwnerRevived);
}

void UCombatantComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(ResourceComponent))
	{
		ResourceComponent->OnDeath.RemoveDynamic(this, &UCombatantComponent::HandleOwnerDeath);
		ResourceComponent->OnRevived.RemoveDynamic(this, &UCombatantComponent::HandleOwnerRevived);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EffectExpiryTimer);
	}
	
	Super::EndPlay(EndPlayReason);
}

bool UCombatantComponent::DeliverPayload(AActor* Target, const FAbilityPayload& Payload)
{
	if (!IsValid(Target))
	{
		return false;
	}

	if (Target->GetClass()->ImplementsInterface(UPayloadReceiver::StaticClass()))
	{
		return IPayloadReceiver::Execute_ReceivePayload(Target, Payload);
	}

	UCombatantComponent* Combatant = FindCombatant(Target);
	return IsValid(Combatant) && Combatant->ReceivePayload(Payload);
}

bool UCombatantComponent::ReceivePayload(const FAbilityPayload& Payload)
{
	if (!CanReceivePayload(Payload))
	{
		return false;
	}

	float DamageApplied = 0.0f;

	if (IsValid(ResourceComponent))
	{
		
		// Defensive buffs and statuses alike, scoped against the attacking ability's tags.
		const float TakenMultiplier = GetModifiedValue(AbilitySystemTags::Stat_DamageTaken, DamageTakenMultiplier, Payload.SourceAbilityTags);
		
		const float Variance = GetDefault<UAbilitySystemSettings>()->DamageVariance;

		const float Damage = FMath::Max(
			Payload.Damage * FMath::Max(TakenMultiplier, 0.0f) * FMath::FRandRange(1.0f - Variance, 1.0f + Variance),
			0.0f
		);
		
		const float HealthBefore = ResourceComponent->GetResourceValue(EResourceType::Health, EResourceValueType::Current);

		// A lethal hit fires OnDeath inside ModifyResource; this lets that callback credit the killer.
		PendingInstigator = Payload.Instigator.Get();
		ResourceComponent->ModifyResource(EResourceType::Health, EResourceValueType::Current, -Damage);
		PendingInstigator = nullptr;

		DamageApplied = HealthBefore - ResourceComponent->GetResourceValue(EResourceType::Health, EResourceValueType::Current);
	}

	// After the damage, so a marking hit doesn't benefit from its own mark.
	if (!IsDead())
	{
		for (const FStatusApplication& Status : Payload.Statuses)
		{
			ApplyStatus(Status, Payload.Instigator.Get());
		}
	}
	if (!IsDead())
	{
		const FHitReactionResult Reaction = ResolveHitReaction(Payload);

		if (Reaction.Reaction != EHitReactionType::None)
		{
			OnHitReaction.Broadcast(Reaction);
		}
	}
	OnPayloadReceived.Broadcast(Payload, DamageApplied);

	if (UCombatantComponent* InstigatorCombatant = FindCombatant(Payload.Instigator.Get()))
	{
		InstigatorCombatant->OnDamageDealt.Broadcast(GetOwner(), Payload, DamageApplied);
	}

	return true;
}

bool UCombatantComponent::CanReceivePayload(const FAbilityPayload& Payload) const
{
	return !IsDead() &&
		!IsInvulnerable() &&
		Payload.Instigator.Get() != GetOwner();
}

bool UCombatantComponent::IsDead() const
{
	return IsValid(ResourceComponent) && ResourceComponent->IsDead();
}

bool UCombatantComponent::IsInvulnerable() const
{
	return bInvulnerable ||
		(IsValid(AbilityComponent) && AbilityComponent->HasOwnerTag(AbilitySystemTags::State_Invulnerable));
}

void UCombatantComponent::HandleOwnerDeath()
{
	// The dead lose every timed effect (statuses, procs); learned passives' permanent bonuses stay.
	if (EffectContainer.RemoveTimed())
	{
		HandleEffectsChanged();
	}
	
	AActor* Killer = PendingInstigator.Get();

	if (IsValid(AbilityComponent))
	{
		// Tag first: nothing started from the cancel callbacks may activate.
		AbilityComponent->AddLooseOwnerTag(AbilitySystemTags::State_Dead);
		AbilityComponent->ClearBufferedInputs();
		AbilityComponent->CancelActiveAbility(false);
	}

	OnDied.Broadcast(Killer);

	if (UCombatantComponent* KillerCombatant = FindCombatant(Killer))
	{
		KillerCombatant->OnKilled.Broadcast(GetOwner());
	}
}

void UCombatantComponent::HandleOwnerRevived()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->RemoveLooseOwnerTag(AbilitySystemTags::State_Dead);
	}

	OnRevived.Broadcast();
}

UCombatantComponent* UCombatantComponent::FindCombatant(const AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UCombatantComponent>() : nullptr;
}


double UCombatantComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}

float UCombatantComponent::GetPoise() const
{
	const double RecoveringFor = GetWorldTime() - (LastPoiseDamageTime + PoiseRegenDelay);
	const float Recovered = RecoveringFor > 0.0 ? static_cast<float>(RecoveringFor) * PoiseRegenRate : 0.0f;

	return FMath::Min(StoredPoise + Recovered, MaxPoise);
}

bool UCombatantComponent::IsStaggerImmune() const
{
	return GetWorldTime() < StaggerImmuneUntil;
}

bool UCombatantComponent::HasCombatTag(const FGameplayTag Tag) const
{
	if (!Tag.IsValid())
	{
		return false;
	}

	if (HasEffectTag(Tag))
	{
		return true;
	}

	return IsValid(AbilityComponent) && AbilityComponent->HasOwnerTag(Tag);
}

FHitReactionResult UCombatantComponent::ResolveHitReaction(const FAbilityPayload& Payload)
{	
	FHitReactionResult Result;
	const FHitImpact& Impact = Payload.Impact;

	if (Impact.Reaction == EHitReactionType::None)
	{
		return Result;
	}

	const double Now = GetWorldTime();

	// Bank any recovery so far, then drain.
	StoredPoise = FMath::Max(GetPoise() - Impact.PoiseDamage, 0.0f);
	LastPoiseDamageTime = Now;

	const bool bBreaksPoise = Impact.bForceReaction || StoredPoise <= 0.0f;
	const bool bCanBreak = !IsStaggerImmune() && !HasCombatTag(AbilitySystemTags::State_SuperArmor);

	if (Impact.Reaction == EHitReactionType::Flinch || !bBreaksPoise || !bCanBreak)
	{
		Result.Reaction = EHitReactionType::Flinch;
	}
	else
	{
		Result.Reaction = Impact.Reaction;
		Result.KnockbackSpeed = Impact.KnockbackSpeed;
		Result.KnockbackLift = Impact.KnockbackLift;

		// Poise refills after a break, and the target gets a moment before it can break again.
		StoredPoise = MaxPoise;
		StaggerImmuneUntil = Now + StaggerImmunityDuration;
	}

	// Direction: from the attacker toward this character, flattened.
	const AActor* Owner = GetOwner();
	AActor* InstigatorActor = Payload.Instigator.Get();

	FVector HitDirection = IsValid(InstigatorActor)
		? Owner->GetActorLocation() - InstigatorActor->GetActorLocation()
		: -Owner->GetActorForwardVector();

	HitDirection.Z = 0.0f;
	HitDirection = HitDirection.GetSafeNormal();

	if (HitDirection.IsNearlyZero())
	{
		HitDirection = -Owner->GetActorForwardVector();
	}

	// The side the hit came from is opposite to the way it travelled.
	const FVector FromDirection = -HitDirection;
	const float ForwardDot = FVector::DotProduct(FromDirection, Owner->GetActorForwardVector());
	const float RightDot = FVector::DotProduct(FromDirection, Owner->GetActorRightVector());

	if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
	{
		Result.Direction = ForwardDot >= 0.0f ? EHitDirection::Front : EHitDirection::Back;
	}
	else
	{
		Result.Direction = RightDot >= 0.0f ? EHitDirection::Right : EHitDirection::Left;
	}

	Result.HitDirection = HitDirection;
	Result.Instigator = InstigatorActor;
	return Result;
}
void UCombatantComponent::ApplyEffect(const UObject* Source, const FName EntryKey, const TArray<FStatModifier>& Modifiers, const int32 Rank, const FGameplayTagContainer& GrantedTags, const float Duration)
{
	const double ExpiresAt = Duration > 0.0f ? GetWorldTime() + Duration : 0.0;

	EffectContainer.Apply(IsValid(Source) ? Source : this, EntryKey, Modifiers, FMath::Max(Rank, 1), GrantedTags, ExpiresAt, Duration);
	HandleEffectsChanged();
}

bool UCombatantComponent::RemoveEffect(const UObject* Source, const FName EntryKey)
{
	if (!EffectContainer.Remove(IsValid(Source) ? Source : this, EntryKey))
	{
		return false;
	}

	HandleEffectsChanged();
	return true;
}

bool UCombatantComponent::RemoveEffectsFromSource(const UObject* Source)
{
	if (!EffectContainer.RemoveAll(IsValid(Source) ? Source : this))
	{
		return false;
	}

	HandleEffectsChanged();
	return true;
}

bool UCombatantComponent::HasEffect(const UObject* Source, const FName EntryKey) const
{
	return EffectContainer.Contains(IsValid(Source) ? Source : this, EntryKey);
}

bool UCombatantComponent::SetEffectStacks(const UObject* Source, const FName EntryKey, const int32 Stacks)
{
	if (!EffectContainer.SetStacks(IsValid(Source) ? Source : this, EntryKey, Stacks))
	{
		return false;
	}

	// No stats changed, but listeners such as the effects bar want the new count.
	OnEffectsChanged.Broadcast();
	return true;
}

void UCombatantComponent::ApplyStatus(const FStatusApplication& Status, const UObject* Source)
{
	if (!Status.StatusTag.IsValid() || IsDead())
	{
		return;
	}

	FGameplayTagContainer Tags = Status.GrantedTags;
	Tags.AddTag(Status.StatusTag);

	const double ExpiresAt = Status.Duration > 0.0f ? GetWorldTime() + Status.Duration : 0.0;

	EffectContainer.Apply(
		IsValid(Source) ? Source : this,
		Status.StatusTag.GetTagName(),
		Status.Modifiers,
		FMath::Max(Status.Rank, 1),
		Tags,
		ExpiresAt,
		Status.Duration,
		Status.Display
	);

	HandleEffectsChanged();
}

float UCombatantComponent::GetModifiedValue(const FGameplayTag Stat, const float BaseValue, const FGameplayTagContainer& AbilityTags) const
{
	return EffectContainer.Evaluate(Stat, BaseValue, AbilityTags, GetOwnerTagsForEvaluation(), GetWorldTime());
}

FGameplayTagContainer UCombatantComponent::GetEffectTags() const
{
	FGameplayTagContainer Tags;
	EffectContainer.AppendGrantedTags(Tags, GetWorldTime());
	return Tags;
}

bool UCombatantComponent::HasEffectTag(const FGameplayTag Tag) const
{
	return Tag.IsValid() && GetEffectTags().HasTag(Tag);
}

void UCombatantComponent::GetActiveEffects(TArray<FActiveEffectInfo>& OutEffects) const
{
	EffectContainer.GetDisplayInfo(OutEffects, GetWorldTime(), false);
}

FGameplayTagContainer UCombatantComponent::GetOwnerTagsForEvaluation() const
{
	// The ability component's owned tags already include effect tags (it reads them from here).
	return IsValid(AbilityComponent) ? AbilityComponent->GetOwnedGameplayTags() : GetEffectTags();
}

void UCombatantComponent::HandleEffectsChanged()
{
	ScheduleEffectExpiry();
	OnEffectsChanged.Broadcast();
}

void UCombatantComponent::ScheduleEffectExpiry()
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(EffectExpiryTimer);

	const double NextExpiry = EffectContainer.GetNextExpiryTime();

	if (NextExpiry <= 0.0)
	{
		return;
	}

	const float Delay = FMath::Max(static_cast<float>(NextExpiry - World->GetTimeSeconds()), UE_KINDA_SMALL_NUMBER);
	TimerManager.SetTimer(EffectExpiryTimer, this, &UCombatantComponent::HandleEffectExpiry, Delay, false);
}

void UCombatantComponent::HandleEffectExpiry()
{
	if (EffectContainer.PruneExpired(GetWorldTime()))
	{
		HandleEffectsChanged();
	}
	else
	{
		ScheduleEffectExpiry();
	}
}