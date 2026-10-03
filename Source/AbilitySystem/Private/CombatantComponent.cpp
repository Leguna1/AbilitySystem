#include "CombatantComponent.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"
#include "ResourceComponent.h"
#include "AbilitySystemSettings.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UCombatantComponent::UCombatantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatantComponent::BeginPlay()
{
	Super::BeginPlay();

	ResourceComponent = GetOwner()->FindComponentByClass<UResourceComponent>();
	AbilityComponent = GetOwner()->FindComponentByClass<UAbilityComponent>();
	
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (const UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
		{
			BaseMaxWalkSpeed = Movement->MaxWalkSpeed;
		}
	}

	// The owner's buffs (Swiftness etc.) change walk speed too.
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->ModifiersChangedEvent.AddDynamic(this, &UCombatantComponent::HandleOwnerModifiersChanged);
	}

	ApplyMovementSpeed();

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
		World->GetTimerManager().ClearTimer(StatusExpiryTimer);
	}
	
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->ModifiersChangedEvent.RemoveDynamic(this, &UCombatantComponent::HandleOwnerModifiersChanged);
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
		// The owner's modifiers (defensive passives) adjust the base multiplier.
		const float TakenMultiplier = IsValid(AbilityComponent)
			? AbilityComponent->GetModifiedValue(AbilitySystemTags::Stat_DamageTaken, DamageTakenMultiplier, FGameplayTagContainer())
			: DamageTakenMultiplier;

		// The target's statuses (e.g. Marked), scoped against the attacking ability's tags.
		const float StatusTakenMultiplier = StatusContainer.Evaluate(
			AbilitySystemTags::Stat_DamageTaken,
			TakenMultiplier,
			Payload.SourceAbilityTags,
			GetStatusTags(),
			GetWorldTime()
		);
		
		const float Variance = GetDefault<UAbilitySystemSettings>()->DamageVariance;

		const float Damage = FMath::Max(
			Payload.Damage * FMath::Max(StatusTakenMultiplier, 0.0f) * FMath::FRandRange(1.0f - Variance, 1.0f + Variance),
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
	StatusContainer.Reset();
	HandleStatusesChanged();
	
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
void UCombatantComponent::ApplyStatus(const FStatusApplication& Status, const UObject* Source)
{
	if (!Status.StatusTag.IsValid() || IsDead())
	{
		return;
	}

	FGameplayTagContainer Tags = Status.GrantedTags;
	Tags.AddTag(Status.StatusTag);

	const double ExpiresAt = Status.Duration > 0.0f ? GetWorldTime() + Status.Duration : 0.0;

	StatusContainer.Apply(
		IsValid(Source) ? Source : this,
		Status.StatusTag.GetTagName(),
		Status.Modifiers,
		FMath::Max(Status.Rank, 1),
		Tags,
		ExpiresAt,
		Status.Duration
	);

	HandleStatusesChanged();
}

bool UCombatantComponent::HasStatusTag(const FGameplayTag Tag) const
{
	return Tag.IsValid() && GetStatusTags().HasTag(Tag);
}

FGameplayTagContainer UCombatantComponent::GetStatusTags() const
{
	FGameplayTagContainer Tags;
	StatusContainer.AppendGrantedTags(Tags, GetWorldTime());
	return Tags;
}

void UCombatantComponent::HandleStatusesChanged()
{
	ApplyMovementSpeed();
	ScheduleStatusExpiry();
	OnStatusesChanged.Broadcast();
}

void UCombatantComponent::ScheduleStatusExpiry()
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	TimerManager.ClearTimer(StatusExpiryTimer);

	const double NextExpiry = StatusContainer.GetNextExpiryTime();

	if (NextExpiry <= 0.0)
	{
		return;
	}

	const float Delay = FMath::Max(static_cast<float>(NextExpiry - World->GetTimeSeconds()), UE_KINDA_SMALL_NUMBER);
	TimerManager.SetTimer(StatusExpiryTimer, this, &UCombatantComponent::HandleStatusExpiry, Delay, false);
}

void UCombatantComponent::HandleStatusExpiry()
{
	if (StatusContainer.PruneExpired(GetWorldTime()))
	{
		HandleStatusesChanged();
	}
	else
	{
		ScheduleStatusExpiry();
	}
}

double UCombatantComponent::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.0;
}
void UCombatantComponent::GetActiveStatuses(TArray<FActiveEffectInfo>& OutStatuses) const
{
	StatusContainer.GetDisplayInfo(OutStatuses, GetWorldTime(), true);
}
void UCombatantComponent::SetBaseMaxWalkSpeed(const float NewBaseSpeed)
{
	BaseMaxWalkSpeed = FMath::Max(NewBaseSpeed, 0.0f);
	ApplyMovementSpeed();
}

void UCombatantComponent::HandleOwnerModifiersChanged()
{
	ApplyMovementSpeed();
}

void UCombatantComponent::ApplyMovementSpeed()
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = IsValid(OwnerCharacter) ? OwnerCharacter->GetCharacterMovement() : nullptr;

	if (!IsValid(Movement) || BaseMaxWalkSpeed <= 0.0f)
	{
		return;
	}

	// Movement isn't an ability, so only unscoped modifiers apply in both layers.
	float Multiplier = IsValid(AbilityComponent)
		? AbilityComponent->GetModifiedValue(AbilitySystemTags::Stat_MoveSpeed, 1.0f, FGameplayTagContainer())
		: 1.0f;

	Multiplier = StatusContainer.Evaluate(
		AbilitySystemTags::Stat_MoveSpeed,
		Multiplier,
		FGameplayTagContainer(),
		GetStatusTags(),
		GetWorldTime()
	);

	Movement->MaxWalkSpeed = BaseMaxWalkSpeed * FMath::Max(Multiplier, 0.0f);
}