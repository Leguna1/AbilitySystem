#include "CombatantComponent.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"
#include "ResourceComponent.h"

UCombatantComponent::UCombatantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatantComponent::BeginPlay()
{
	Super::BeginPlay();

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
		const float Damage = FMath::Max(Payload.Damage * DamageTakenMultiplier, 0.0f);
		const float HealthBefore = ResourceComponent->GetResourceValue(EResourceType::Health, EResourceValueType::Current);

		// A lethal hit fires OnDeath inside ModifyResource; this lets that callback credit the killer.
		PendingInstigator = Payload.Instigator.Get();
		ResourceComponent->ModifyResource(EResourceType::Health, EResourceValueType::Current, -Damage);
		PendingInstigator = nullptr;

		DamageApplied = HealthBefore - ResourceComponent->GetResourceValue(EResourceType::Health, EResourceValueType::Current);
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