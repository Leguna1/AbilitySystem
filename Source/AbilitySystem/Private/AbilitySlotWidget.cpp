#include "AbilitySlotWidget.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "AbilityDragDropOperation.h"
#include "ActionBarComponent.h"
#include "ActiveAbility.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "TimerManager.h"

void UAbilitySlotWidget::InitializeSlot(UAbilityComponent* InAbilityComponent, const TSubclassOf<UAbility> InAbilityClass)
{
	Unbind();
	ResetRuntimeState();

	AbilityComponent = InAbilityComponent;
	AbilityClass = InAbilityClass;

	const UAbility* Defaults = IsValid(AbilityComponent) ? AbilityComponent->GetAbilityDefaults(AbilityClass) : nullptr;

	if (!IsValid(Defaults))
	{
		AbilityClass = nullptr;
	}
	else
	{
		SlotAbilityId = Defaults->GetAbilityId();
		DisplayName = Defaults->GetDisplayName();
		Description = Defaults->GetDescription();
		Icon = Defaults->GetIcon();

		if (const UActiveAbility* ActiveDefaults = Cast<UActiveAbility>(Defaults))
		{
			FocusCost = ActiveDefaults->GetFocusCost();

			// On a bar the slot owns the key; elsewhere the ability's own label.
			if (!IsValid(ActionBar))
			{
				KeybindLabel = ActiveDefaults->GetKeybindLabel();
			}
		}

		Bind();

		const UAbility* Active = AbilityComponent->GetActiveAbility();
		SetActive(IsValid(Active) && Active->GetAbilityId().MatchesTagExact(SlotAbilityId));

		RefreshAffordability();

		if (AbilityComponent->IsAbilityOnCooldown(SlotAbilityId))
		{
			BeginCooldownPoll();
		}
	}

	ToolTipWidgetDelegate.Unbind();

	if (!IsEmpty())
	{
		// Built on hover, not up front.
		ToolTipWidgetDelegate.BindDynamic(this, &UAbilitySlotWidget::GetTooltipWidget);
	}

	NativeOnSlotChanged();
	OnSlotEmptyChanged(IsEmpty());
	OnSlotInitialized();
}

bool UAbilitySlotWidget::IsEmpty() const
{
	// Here, not inline in the header: TSubclassOf needs the full UAbility type to test itself.
	return AbilityClass.Get() == nullptr;
}

void UAbilitySlotWidget::SetActionBarSlot(UActionBarComponent* InActionBar, const int32 InSlotIndex)
{
	ActionBar = InActionBar;
	SlotIndex = InSlotIndex;

	KeybindLabel = IsValid(ActionBar) ? ActionBar->GetSlotDefinition(SlotIndex).KeybindLabel : FText::GetEmpty();
}

void UAbilitySlotWidget::NativeDestruct()
{
	Unbind();
	StopCooldownPoll();
	Super::NativeDestruct();
}

/* -------------------- Editing: right-click, drag and drop -------------------- */

FReply UAbilitySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsValid(ActionBar) || IsEmpty())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FKey Button = InMouseEvent.GetEffectingButton();

	if (Button == EKeys::RightMouseButton && bRightClickRemoves)
	{
		ActionBar->ClearSlot(SlotIndex);
		return FReply::Handled();
	}

	if (Button == EKeys::LeftMouseButton)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UAbilitySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (IsEmpty() || !IsValid(ActionBar))
	{
		return;
	}

	UAbilityDragDropOperation* Operation = NewObject<UAbilityDragDropOperation>(this);
	Operation->AbilityClass = AbilityClass;
	Operation->SourceSlotIndex = SlotIndex;
	Operation->SourceActionBar = ActionBar;
	Operation->Pivot = EDragPivot::CenterCenter;

	UWidget* Visual = BuildDragVisual();

	if (!IsValid(Visual) && IsValid(Icon))
	{
		UImage* IconImage = NewObject<UImage>(this);
		IconImage->SetBrushFromTexture(Icon);
		IconImage->SetDesiredSizeOverride(DragIconSize);
		Visual = IconImage;
	}

	Operation->DefaultDragVisual = Visual;
	OutOperation = Operation;
}

bool UAbilitySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	OnDragHoverChanged(false);

	const UAbilityDragDropOperation* Operation = Cast<UAbilityDragDropOperation>(InOperation);

	if (!Operation || !IsValid(ActionBar))
	{
		return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	}

	// From another slot of this bar: swap. From the skill tree: place here.
	// From a different bar (the companion's onto yours): nothing, each bar keeps its own abilities.
	if (Operation->SourceSlotIndex != INDEX_NONE)
	{
		if (Operation->SourceActionBar.Get() == ActionBar)
		{
			ActionBar->MoveSlot(Operation->SourceSlotIndex, SlotIndex);
		}
	}
	else
	{
		ActionBar->AssignAbility(SlotIndex, Operation->AbilityClass);
	}

	// Handled even when refused (e.g. not learned): dropping on a slot never empties the source.
	return true;
}

void UAbilitySlotWidget::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);

	if (IsValid(ActionBar) && Cast<UAbilityDragDropOperation>(InOperation))
	{
		OnDragHoverChanged(true);
	}
}

void UAbilitySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	OnDragHoverChanged(false);
}

void UAbilitySlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);

	// Let go somewhere that isn't a slot: take it off the bar.
	const UAbilityDragDropOperation* Operation = Cast<UAbilityDragDropOperation>(InOperation);

	if (bRemoveWhenDraggedOff && Operation && IsValid(ActionBar) && Operation->SourceSlotIndex == SlotIndex)
	{
		ActionBar->ClearSlot(SlotIndex);
	}
}

/* -------------------- Runtime state -------------------- */

UWidget* UAbilitySlotWidget::GetTooltipWidget()
{
	return BuildTooltipWidget();
}

void UAbilitySlotWidget::Bind()
{
	if (!IsValid(AbilityComponent))
	{
		return;
	}

	AbilityComponent->AbilityActivatedEvent.AddUniqueDynamic(this, &UAbilitySlotWidget::HandleAbilityActivated);
	AbilityComponent->AbilityEndedEvent.AddUniqueDynamic(this, &UAbilitySlotWidget::HandleAbilityEnded);
	AbilityComponent->AbilityCommittedEvent.AddUniqueDynamic(this, &UAbilitySlotWidget::HandleAbilityCommitted);
	AbilityComponent->AbilityCooldownStartedEvent.AddUniqueDynamic(this, &UAbilitySlotWidget::HandleCooldownStarted);

	if (UResourceComponent* Resources = AbilityComponent->GetResourceComponent())
	{
		Resources->OnResourceChanged.AddUniqueDynamic(this, &UAbilitySlotWidget::HandleResourceChanged);
	}
}

void UAbilitySlotWidget::Unbind()
{
	if (!IsValid(AbilityComponent))
	{
		return;
	}

	AbilityComponent->AbilityActivatedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityActivated);
	AbilityComponent->AbilityEndedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityEnded);
	AbilityComponent->AbilityCommittedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleAbilityCommitted);
	AbilityComponent->AbilityCooldownStartedEvent.RemoveDynamic(this, &UAbilitySlotWidget::HandleCooldownStarted);

	if (UResourceComponent* Resources = AbilityComponent->GetResourceComponent())
	{
		Resources->OnResourceChanged.RemoveDynamic(this, &UAbilitySlotWidget::HandleResourceChanged);
	}
}

void UAbilitySlotWidget::ResetRuntimeState()
{
	StopCooldownPoll();
	SetCooldownState(false);
	SetActive(false);
	SetAffordable(true);

	SlotAbilityId = FGameplayTag();
	DisplayName = FText::GetEmpty();
	Description = FText::GetEmpty();
	Icon = nullptr;
	FocusCost = 0.0f;
}

void UAbilitySlotWidget::HandleAbilityActivated(const FGameplayTag AbilityId, UActiveAbility* Ability)
{
	if (AbilityId.MatchesTagExact(SlotAbilityId))
	{
		SetActive(true);
	}
}

void UAbilitySlotWidget::HandleAbilityEnded(const FGameplayTag AbilityId, UActiveAbility* Ability, const EAbilityEndReason EndReason)
{
	if (AbilityId.MatchesTagExact(SlotAbilityId))
	{
		SetActive(false);

		// Cooldowns that start when the ability ends.
		if (IsValid(AbilityComponent) && AbilityComponent->IsAbilityOnCooldown(SlotAbilityId) && !bOnCooldown)
		{
			BeginCooldownPoll();
		}
	}
}

void UAbilitySlotWidget::HandleAbilityCommitted(const FGameplayTag AbilityId, UActiveAbility* Ability)
{
	if (!AbilityId.MatchesTagExact(SlotAbilityId))
	{
		return;
	}

	if (IsValid(AbilityComponent) && AbilityComponent->IsAbilityOnCooldown(SlotAbilityId))
	{
		BeginCooldownPoll();
	}

	RefreshAffordability();
}

void UAbilitySlotWidget::HandleCooldownStarted(const FGameplayTag AbilityId, float Duration)
{
	if (AbilityId.MatchesTagExact(SlotAbilityId))
	{
		// Restart even if already sweeping: the new cooldown may be longer.
		StopCooldownPoll();
		BeginCooldownPoll();
	}
}

void UAbilitySlotWidget::HandleResourceChanged(const EResourceType ResourceType, const EResourceValueType ValueType, float OldValue, float NewValue)
{
	if (ResourceType == EResourceType::Focus && ValueType == EResourceValueType::Current)
	{
		RefreshAffordability();
	}
}

void UAbilitySlotWidget::SetActive(const bool bNewActive)
{
	if (bIsActive != bNewActive)
	{
		bIsActive = bNewActive;
		OnActiveStateChanged(bIsActive);
	}
}

void UAbilitySlotWidget::BeginCooldownPoll()
{
	SetCooldownState(true);
	TickCooldown();

	if (bOnCooldown)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(CooldownTimerHandle, this, &UAbilitySlotWidget::TickCooldown,
				FMath::Max(CooldownPollInterval, 0.01f), true);
		}
	}
}

void UAbilitySlotWidget::StopCooldownPoll()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}
}

void UAbilitySlotWidget::TickCooldown()
{
	if (!IsValid(AbilityComponent))
	{
		StopCooldownPoll();
		SetCooldownState(false);
		return;
	}

	const float Remaining = AbilityComponent->GetAbilityCooldownRemaining(SlotAbilityId);
	OnCooldownUpdated(Remaining, AbilityComponent->GetAbilityCooldownProgress(SlotAbilityId));

	if (Remaining <= 0.0f)
	{
		StopCooldownPoll();
		SetCooldownState(false);
	}
}

void UAbilitySlotWidget::RefreshAffordability()
{
	SetAffordable(!IsValid(AbilityComponent) || IsEmpty() || AbilityComponent->CanAffordAbility(SlotAbilityId));
}

void UAbilitySlotWidget::SetCooldownState(const bool bNewOnCooldown)
{
	if (bOnCooldown != bNewOnCooldown)
	{
		bOnCooldown = bNewOnCooldown;
		OnCooldownStateChanged(bOnCooldown);
	}
}

void UAbilitySlotWidget::SetAffordable(const bool bNewCanAfford)
{
	if (bCanAfford != bNewCanAfford)
	{
		bCanAfford = bNewCanAfford;
		OnAffordabilityChanged(bCanAfford);
	}
}
