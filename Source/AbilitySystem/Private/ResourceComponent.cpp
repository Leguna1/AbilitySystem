#include "ResourceComponent.h"

#include "AbilitySystemTags.h"
#include "AttributeComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UResourceComponent::UResourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	MaxHealthAttribute = AbilitySystemTags::Stat_MaxHealth;
	MaxFocusAttribute = AbilitySystemTags::Stat_MaxFocus;
}

void UResourceComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxHealth = FMath::Max(0.0f, MaxHealth);
	CurrentHealth = FMath::Clamp(
		CurrentHealth,
		0.0f,
		MaxHealth
	);

	MaxFocus = FMath::Max(0.0f, MaxFocus);
	CurrentFocus = FMath::Clamp(
		CurrentFocus,
		0.0f,
		MaxFocus
	);

	bIsDead = CurrentHealth <= 0.0f;

	// Max values follow the attributes from here on. If the Attribute Component starts after us,
	// its first recalculation arrives as an attribute change and is applied the same way.
	AttributeComponent = GetOwner()->FindComponentByClass<UAttributeComponent>();

	if (IsValid(AttributeComponent))
	{
		InitializeMaxFromAttribute(EResourceType::Health, MaxHealthAttribute);
		InitializeMaxFromAttribute(EResourceType::Focus, MaxFocusAttribute);

		AttributeComponent->OnAttributeChanged.AddDynamic(this, &UResourceComponent::HandleAttributeChanged);
	}

	RestartFocusRegeneration();
}

void UResourceComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	StopAllRegeneration();

	if (IsValid(AttributeComponent))
	{
		AttributeComponent->OnAttributeChanged.RemoveDynamic(this, &UResourceComponent::HandleAttributeChanged);
	}

	Super::EndPlay(EndPlayReason);
}

float UResourceComponent::GetResourceValue(
	const EResourceType ResourceType,
	const EResourceValueType ValueType) const
{
	switch (ResourceType)
	{
	case EResourceType::Health:
		return ValueType == EResourceValueType::Current
			? CurrentHealth
			: MaxHealth;

	case EResourceType::Focus:
		return ValueType == EResourceValueType::Current
			? CurrentFocus
			: MaxFocus;

	default:
		return 0.0f;
	}
}

float UResourceComponent::ModifyResource(
	const EResourceType ResourceType,
	const EResourceValueType ValueType,
	const float ModifyValue)
{
	if (FMath::IsNearlyZero(ModifyValue))
	{
		return GetResourceValue(ResourceType, ValueType);
	}

	if (ValueType == EResourceValueType::Current)
	{
		return ModifyCurrentResource(
			ResourceType,
			ModifyValue
		);
	}

	return ModifyMaxResource(
		ResourceType,
		ModifyValue
	);
}

float UResourceComponent::ModifyCurrentResource(
	const EResourceType ResourceType,
	const float ModifyValue)
{
	switch (ResourceType)
	{
	case EResourceType::Health:
	{
		// The dead stay dead until Revive; healing is ignored (before changing anything).
		if (bIsDead && ModifyValue > 0.0f)
		{
			return CurrentHealth;
		}

		const float OldHealth = CurrentHealth;

		CurrentHealth = FMath::Clamp(
			CurrentHealth + ModifyValue,
			0.0f,
			MaxHealth
		);

		if (!FMath::IsNearlyEqual(OldHealth, CurrentHealth))
		{
			BroadcastResourceChange(
				EResourceType::Health,
				EResourceValueType::Current,
				OldHealth,
				CurrentHealth
			);

			if (CurrentHealth < OldHealth)
			{
				HandleHealthReduced();
			}

			EvaluateDeath(
				OldHealth,
				CurrentHealth
			);
		}

		return CurrentHealth;
	}

	case EResourceType::Focus:
	{
		const float OldFocus = CurrentFocus;

		CurrentFocus = FMath::Clamp(
			CurrentFocus + ModifyValue,
			0.0f,
			MaxFocus
		);

		if (!FMath::IsNearlyEqual(OldFocus, CurrentFocus))
		{
			BroadcastResourceChange(
				EResourceType::Focus,
				EResourceValueType::Current,
				OldFocus,
				CurrentFocus
			);
		}

		return CurrentFocus;
	}

	default:
		return 0.0f;
	}
}

float UResourceComponent::ModifyMaxResource(
	const EResourceType ResourceType,
	const float ModifyValue)
{
	switch (ResourceType)
	{
	case EResourceType::Health:
	{
		const float OldMaxHealth = MaxHealth;
		const float OldCurrentHealth = CurrentHealth;

		MaxHealth = FMath::Max(
			0.0f,
			MaxHealth + ModifyValue
		);

		CurrentHealth = FMath::Clamp(
			CurrentHealth,
			0.0f,
			MaxHealth
		);

		if (!FMath::IsNearlyEqual(
			OldMaxHealth,
			MaxHealth))
		{
			BroadcastResourceChange(
				EResourceType::Health,
				EResourceValueType::Max,
				OldMaxHealth,
				MaxHealth
			);
		}

		if (!FMath::IsNearlyEqual(
			OldCurrentHealth,
			CurrentHealth))
		{
			BroadcastResourceChange(
				EResourceType::Health,
				EResourceValueType::Current,
				OldCurrentHealth,
				CurrentHealth
			);

			if (CurrentHealth < OldCurrentHealth)
			{
				HandleHealthReduced();
			}

			EvaluateDeath(
				OldCurrentHealth,
				CurrentHealth
			);
		}

		return MaxHealth;
	}

	case EResourceType::Focus:
	{
		const float OldMaxFocus = MaxFocus;
		const float OldCurrentFocus = CurrentFocus;

		MaxFocus = FMath::Max(
			0.0f,
			MaxFocus + ModifyValue
		);

		CurrentFocus = FMath::Clamp(
			CurrentFocus,
			0.0f,
			MaxFocus
		);

		if (!FMath::IsNearlyEqual(
			OldMaxFocus,
			MaxFocus))
		{
			BroadcastResourceChange(
				EResourceType::Focus,
				EResourceValueType::Max,
				OldMaxFocus,
				MaxFocus
			);
		}

		if (!FMath::IsNearlyEqual(
			OldCurrentFocus,
			CurrentFocus))
		{
			BroadcastResourceChange(
				EResourceType::Focus,
				EResourceValueType::Current,
				OldCurrentFocus,
				CurrentFocus
			);
		}

		return MaxFocus;
	}

	default:
		return 0.0f;
	}
}

void UResourceComponent::HandleHealthReduced()
{
	StopHealthRegeneration();

	if (!bEnableHealthRegeneration ||
		bIsDead ||
		CurrentHealth <= 0.0f ||
		CurrentHealth >= MaxHealth)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	if (HealthRegenerationDelay <= 0.0f)
	{
		StartHealthRegeneration();
		return;
	}

	World->GetTimerManager().SetTimer(
		HealthRegenerationDelayTimer,
		this,
		&UResourceComponent::StartHealthRegeneration,
		HealthRegenerationDelay,
		false
	);
}

void UResourceComponent::StartHealthRegeneration()
{
	if (!bEnableHealthRegeneration ||
		bIsDead ||
		CurrentHealth <= 0.0f ||
		CurrentHealth >= MaxHealth)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float SafeInterval = FMath::Max(
		HealthRegenerationInterval,
		0.01f
	);

	World->GetTimerManager().SetTimer(
		HealthRegenerationTimer,
		this,
		&UResourceComponent::RegenerateHealth,
		SafeInterval,
		true
	);
}

void UResourceComponent::RegenerateHealth()
{
	if (!bEnableHealthRegeneration ||
		bIsDead ||
		CurrentHealth <= 0.0f)
	{
		StopHealthRegeneration();
		return;
	}

	if (CurrentHealth >= MaxHealth)
	{
		StopHealthRegeneration();
		return;
	}

	const float RegenerationAmount =
		MaxHealth *
		(HealthRegenerationPercentOfMax / 100.0f);

	if (RegenerationAmount <= 0.0f)
	{
		StopHealthRegeneration();
		return;
	}

	ModifyResource(
		EResourceType::Health,
		EResourceValueType::Current,
		RegenerationAmount
	);

	if (CurrentHealth >= MaxHealth)
	{
		StopHealthRegeneration();
	}
}

void UResourceComponent::RestartFocusRegeneration()
{
	StopFocusRegeneration();

	if (!bEnableFocusRegeneration)
	{
		return;
	}

	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	const float SafeInterval = FMath::Max(
		FocusRegenerationInterval,
		0.01f
	);

	World->GetTimerManager().SetTimer(
		FocusRegenerationTimer,
		this,
		&UResourceComponent::RegenerateFocus,
		SafeInterval,
		true
	);
}

void UResourceComponent::RegenerateFocus()
{
	if (!bEnableFocusRegeneration)
	{
		StopFocusRegeneration();
		return;
	}

	if (CurrentFocus >= MaxFocus)
	{
		return;
	}

	if (FocusRegenerationAmount <= 0.0f)
	{
		return;
	}

	ModifyResource(
		EResourceType::Focus,
		EResourceValueType::Current,
		FocusRegenerationAmount
	);
}

void UResourceComponent::StopHealthRegeneration()
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager =
		World->GetTimerManager();

	TimerManager.ClearTimer(
		HealthRegenerationDelayTimer
	);

	TimerManager.ClearTimer(
		HealthRegenerationTimer
	);
}

void UResourceComponent::StopFocusRegeneration()
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(
		FocusRegenerationTimer
	);
}

void UResourceComponent::StopAllRegeneration()
{
	StopHealthRegeneration();
	StopFocusRegeneration();
}

void UResourceComponent::BroadcastResourceChange(
	const EResourceType ResourceType,
	const EResourceValueType ValueType,
	const float OldValue,
	const float NewValue)
{
	OnResourceChanged.Broadcast(
		ResourceType,
		ValueType,
		OldValue,
		NewValue
	);
}

void UResourceComponent::EvaluateDeath(
	const float OldHealth,
	const float NewHealth)
{
	if (OldHealth > 0.0f &&
		NewHealth <= 0.0f &&
		!bIsDead)
	{
		bIsDead = true;

		StopHealthRegeneration();

		OnDeath.Broadcast();
	}
}
bool UResourceComponent::Revive(const float HealthFraction)
{
	if (!bIsDead)
	{
		return false;
	}

	bIsDead = false;

	const float OldHealth = CurrentHealth;
	const float TargetHealth = MaxHealth * FMath::Clamp(HealthFraction, 0.0f, 1.0f);

	CurrentHealth = FMath::Min(FMath::Max(TargetHealth, 1.0f), MaxHealth);

	BroadcastResourceChange(EResourceType::Health, EResourceValueType::Current, OldHealth, CurrentHealth);
	OnRevived.Broadcast();
	return true;
}

void UResourceComponent::RefillResources()
{
	if (!bIsDead)
	{
		ModifyResource(EResourceType::Health, EResourceValueType::Current, MaxHealth - CurrentHealth);
	}

	ModifyResource(EResourceType::Focus, EResourceValueType::Current, MaxFocus - CurrentFocus);
}

void UResourceComponent::HandleAttributeChanged(const FGameplayTag Attribute, const float OldValue, const float NewValue)
{
	if (Attribute.IsValid() && Attribute == MaxHealthAttribute)
	{
		ApplyMaxFromAttribute(EResourceType::Health, NewValue);
	}
	else if (Attribute.IsValid() && Attribute == MaxFocusAttribute)
	{
		ApplyMaxFromAttribute(EResourceType::Focus, NewValue);
	}
}

void UResourceComponent::ApplyMaxFromAttribute(const EResourceType ResourceType, const float NewMax)
{
	const float OldMax = GetResourceValue(ResourceType, EResourceValueType::Max);
	const float Difference = FMath::Max(NewMax, 0.0f) - OldMax;

	ModifyResource(ResourceType, EResourceValueType::Max, Difference);

	// More max = the same amount more current: +30 Max Health on level up isn't 30 missing health.
	if (Difference > 0.0f)
	{
		ModifyResource(ResourceType, EResourceValueType::Current, Difference);
	}
}

void UResourceComponent::InitializeMaxFromAttribute(const EResourceType ResourceType, const FGameplayTag Attribute)
{
	if (!Attribute.IsValid() || !AttributeComponent->HasAttribute(Attribute))
	{
		return;
	}

	float& Max = ResourceType == EResourceType::Health ? MaxHealth : MaxFocus;
	float& Current = ResourceType == EResourceType::Health ? CurrentHealth : CurrentFocus;

	const float Fraction = Max > 0.0f ? Current / Max : 1.0f;

	Max = FMath::Max(AttributeComponent->GetAttributeValue(Attribute), 0.0f);
	Current = FMath::Clamp(Max * Fraction, 0.0f, Max);
}
