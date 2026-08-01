#include "AbilitySlotWidget.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "Components/Widget.h"

void UAbilitySlotWidget::InitializeSlot(UAbilityComponent* InAbilityComponent, TSubclassOf<UAbility> InAbilityClass)
{
	// Detach from any previous component before rebinding.
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->AbilityActivatedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityActivated);
		AbilityComponent->AbilityEndedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityEnded);
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

	AbilityComponent->AbilityActivatedEvent.AddDynamic(this, &UAbilitySlotWidget::HandleAbilityActivated);
	AbilityComponent->AbilityEndedEvent.AddDynamic(this, &UAbilitySlotWidget::HandleAbilityEnded);

	// Reflect current state in case this ability is already active at bind time.
	const UAbility* Active = AbilityComponent->GetActiveAbility();
	SetActive(IsValid(Active) && Active->GetAbilityId().MatchesTagExact(SlotAbilityId));

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