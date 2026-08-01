#include "AbilitySlotWidget.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "Components/Widget.h"
#include "TimerManager.h"

void UAbilitySlotWidget::InitializeSlot(UAbilityComponent* InAbilityComponent, TSubclassOf<UAbility> InAbilityClass)
{
	// Detach from any previous component before rebinding.
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityActivatedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityActivated);
		AbilityComponent->AbilityEndedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityEnded);
		AbilityComponent->AbilityCommittedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityCommitted);

		if (UResourceComponent* Resources = AbilityComponent->GetResourceComponent())
		{
			Resources->OnResourceChanged.RemoveDynamic(this, &UAbilitySlotWidget::HandleResourceChanged);
		}
	}

	AbilityComponent = InAbilityComponent;
	AbilityClass = InAbilityClass;

	if (!IsValid(AbilityComponent) || !IsValid(AbilityClass))
	{
		return;
	}

	const UAbility* Defaults = AbilityComponent->GetAbilityDefaults(AbilityClass);
	if (!IsValid(Defaults))
	{
		return;
	}

	SlotAbilityId = Defaults->GetAbilityId();
	DisplayName = Defaults->GetDisplayName();
	Description = Defaults->GetDescription();
	Icon = Defaults->GetIcon();
	ActivationInputTag = Defaults->GetActivationInputTag();
	KeybindLabel = Defaults->GetKeybindLabel();
	FocusCost = Defaults->GetFocusCost();

	AbilityComponent->AbilityActivatedEvent.AddDynamic(this, &UAbilitySlotWidget::HandleAbilityActivated);
	AbilityComponent->AbilityEndedEvent.AddDynamic(this, &UAbilitySlotWidget::HandleAbilityEnded);
	AbilityComponent->AbilityCommittedEvent.AddDynamic(this, &UAbilitySlotWidget::HandleAbilityCommitted);

	if (UResourceComponent* Resources = AbilityComponent->GetResourceComponent())
	{
		Resources->OnResourceChanged.AddDynamic(this, &UAbilitySlotWidget::HandleResourceChanged);
	}

	// Reflect current state in case this ability is already active at bind time.
	const UAbility* Active = AbilityComponent->GetActiveAbility();
	SetActive(IsValid(Active) && Active->GetAbilityId().MatchesTagExact(SlotAbilityId));

	// Seed affordability, and resume a sweep if this ability is already cooling down.
	RefreshAffordability();
	if (AbilityComponent->IsAbilityOnCooldown(SlotAbilityId))
	{
		BeginCooldownPoll();
	}

	OnSlotInitialized();

	// Defer tooltip creation to hover so we don't build a widget per slot up-front.
	ToolTipWidgetDelegate.BindDynamic(this, &UAbilitySlotWidget::GetTooltipWidget);
}

UWidget* UAbilitySlotWidget::GetTooltipWidget()
{
	return BuildTooltipWidget();
}

void UAbilitySlotWidget::NativeDestruct()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityActivatedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityActivated);
		AbilityComponent->AbilityEndedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityEnded);
		AbilityComponent->AbilityCommittedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityCommitted);

		if (UResourceComponent* Resources = AbilityComponent->GetResourceComponent())
		{
			Resources->OnResourceChanged.RemoveDynamic(this, &UAbilitySlotWidget::HandleResourceChanged);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}

	Super::NativeDestruct();
}

void UAbilitySlotWidget::HandleAbilityActivated(FGameplayTag AbilityId, UAbility* Ability)
{
	if (AbilityId.MatchesTagExact(SlotAbilityId))
	{
		SetActive(true);
	}
}

void UAbilitySlotWidget::HandleAbilityEnded(FGameplayTag AbilityId, UAbility* Ability, EAbilityEndReason EndReason)
{
	if (AbilityId.MatchesTagExact(SlotAbilityId))
	{
		SetActive(false);
	}
}

void UAbilitySlotWidget::SetActive(bool bNewActive)
{
	if (bIsActive == bNewActive)
	{
		return;
	}

	bIsActive = bNewActive;
	OnActiveStateChanged(bIsActive);
}

void UAbilitySlotWidget::HandleAbilityCommitted(FGameplayTag AbilityId, UAbility* Ability)
{
	if (!AbilityId.MatchesTagExact(SlotAbilityId))
	{
		return;
	}

	// Commit is when cost is spent and cooldown starts. Begin the sweep if one
	// actually started; also refresh affordability since focus was just spent.
	if (IsValid(AbilityComponent) && AbilityComponent->IsAbilityOnCooldown(SlotAbilityId))
	{
		BeginCooldownPoll();
	}

	RefreshAffordability();
}

void UAbilitySlotWidget::HandleResourceChanged(EResourceType ResourceType, EResourceValueType ValueType, float OldValue, float NewValue)
{
	// Only focus current-value changes affect whether we can afford this ability.
	if (ResourceType != EResourceType::Focus || ValueType != EResourceValueType::Current)
	{
		return;
	}

	RefreshAffordability();
}

void UAbilitySlotWidget::BeginCooldownPoll()
{
	SetCooldownState(true);

	// Push an immediate frame so the sweep starts full rather than waiting a tick.
	TickCooldown();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CooldownTimerHandle, this, &UAbilitySlotWidget::TickCooldown,
			FMath::Max(CooldownPollInterval, 0.01f), true);
	}
}

void UAbilitySlotWidget::TickCooldown()
{
	if (!IsValid(AbilityComponent))
	{
		SetCooldownState(false);
		return;
	}

	const float Remaining = AbilityComponent->GetAbilityCooldownRemaining(SlotAbilityId);
	const float Progress = AbilityComponent->GetAbilityCooldownProgress(SlotAbilityId);

	OnCooldownUpdated(Remaining, Progress);

	if (Remaining <= 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CooldownTimerHandle);
		}

		SetCooldownState(false);
	}
}

void UAbilitySlotWidget::RefreshAffordability()
{
	const bool bAfford = !IsValid(AbilityComponent) || AbilityComponent->CanAffordAbility(SlotAbilityId);
	SetAffordable(bAfford);
}

void UAbilitySlotWidget::SetCooldownState(bool bNewOnCooldown)
{
	if (bOnCooldown == bNewOnCooldown)
	{
		return;
	}

	bOnCooldown = bNewOnCooldown;
	OnCooldownStateChanged(bOnCooldown);
}

void UAbilitySlotWidget::SetAffordable(bool bNewCanAfford)
{
	if (bCanAfford == bNewCanAfford)
	{
		return;
	}

	bCanAfford = bNewCanAfford;
	OnAffordabilityChanged(bCanAfford);
}