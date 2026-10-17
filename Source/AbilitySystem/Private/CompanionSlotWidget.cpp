#include "CompanionSlotWidget.h"

#include "AbilityComponent.h"
#include "ActionBarComponent.h"
#include "CompanionComponent.h"
#include "InputCoreTypes.h"

UCompanionSlotWidget::UCompanionSlotWidget()
{
	// Right-click is auto-cast here, not "remove".
	bRightClickRemoves = false;
	bRemoveWhenDraggedOff = false;
}

UCompanionComponent* UCompanionSlotWidget::FindCompanionComponent() const
{
	// The companion was spawned with its leader as owner.
	const UAbilityComponent* CompanionAbilities = GetSlotAbilityComponent();
	const AActor* Companion = IsValid(CompanionAbilities) ? CompanionAbilities->GetOwner() : nullptr;
	const AActor* Leader = IsValid(Companion) ? Companion->GetOwner() : nullptr;

	return IsValid(Leader) ? Leader->FindComponentByClass<UCompanionComponent>() : nullptr;
}

void UCompanionSlotWidget::BindCompanionComponent()
{
	UCompanionComponent* CompanionComponent = FindCompanionComponent();

	if (CompanionComponent == BoundCompanionComponent.Get())
	{
		return;
	}

	if (UCompanionComponent* Previous = BoundCompanionComponent.Get())
	{
		Previous->OnAutoCastChanged.RemoveDynamic(this, &UCompanionSlotWidget::HandleAutoCastChanged);
	}

	BoundCompanionComponent = CompanionComponent;

	if (IsValid(CompanionComponent))
	{
		CompanionComponent->OnAutoCastChanged.AddUniqueDynamic(this, &UCompanionSlotWidget::HandleAutoCastChanged);
	}
}

void UCompanionSlotWidget::NativeOnSlotChanged()
{
	Super::NativeOnSlotChanged();

	BindCompanionComponent();
	OnAutoCastChanged(IsAutoCast());
}

void UCompanionSlotWidget::NativeDestruct()
{
	if (UCompanionComponent* Previous = BoundCompanionComponent.Get())
	{
		Previous->OnAutoCastChanged.RemoveDynamic(this, &UCompanionSlotWidget::HandleAutoCastChanged);
	}

	BoundCompanionComponent.Reset();
	Super::NativeDestruct();
}

bool UCompanionSlotWidget::IsAutoCast() const
{
	const UCompanionComponent* CompanionComponent = BoundCompanionComponent.Get();
	return !IsEmpty() && IsValid(CompanionComponent) && CompanionComponent->IsAutoCast(GetSlotAbilityId());
}

void UCompanionSlotWidget::HandleAutoCastChanged(const FGameplayTag AbilityId, const bool bAutoCast)
{
	if (AbilityId.MatchesTagExact(GetSlotAbilityId()))
	{
		OnAutoCastChanged(bAutoCast);
	}
}

FReply UCompanionSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (IsEmpty())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FKey Button = InMouseEvent.GetEffectingButton();

	if (Button == EKeys::RightMouseButton)
	{
		if (UCompanionComponent* CompanionComponent = BoundCompanionComponent.Get())
		{
			CompanionComponent->ToggleAutoCast(GetSlotAbilityId());
		}

		return FReply::Handled();
	}

	if (Button == EKeys::LeftMouseButton)
	{
		bPendingClick = true;

		// No bar to rearrange: a plain click.
		if (!IsValid(GetSlotActionBar()))
		{
			return FReply::Handled();
		}
	}

	// Left press on a bar: the base starts drag detection.
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCompanionSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingClick)
	{
		bPendingClick = false;

		if (UCompanionComponent* CompanionComponent = BoundCompanionComponent.Get())
		{
			CompanionComponent->CommandAbility(GetSlotAbilityId());
		}

		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UCompanionSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bPendingClick = false;
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
}
