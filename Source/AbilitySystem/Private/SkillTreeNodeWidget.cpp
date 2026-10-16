#include "SkillTreeNodeWidget.h"

#include "Ability.h"
#include "SkillTreeAsset.h"
#include "InputCoreTypes.h"
#include "AbilityDragDropOperation.h"
#include "ActionBarComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"

void USkillTreeNodeWidget::InitializeNode(USkillTreeComponent* InTreeComponent, FGameplayTag InNodeId)
{
	TreeComponent = InTreeComponent;
	NodeId = InNodeId;

	if (!IsValid(TreeComponent))
	{
		return;
	}

	const USkillTreeAsset* Tree = TreeComponent->GetTreeAsset();
	if (!IsValid(Tree))
	{
		return;
	}

	const FSkillTreeNode* Node = Tree->FindNode(NodeId);
	if (!Node)
	{
		return;
	}

	Kind = Node->Kind;
	AbilityClass = Node->AbilityClass;
	Cost = Node->Cost;
	CanvasPosition = Node->CanvasPosition;

	// Display data comes from the granted ability's class defaults (the CDO),
	// the same source the ability bar reads. No instance is created.
	if (IsValid(Node->AbilityClass))
	{
		if (const UAbility* Defaults = Node->AbilityClass.GetDefaultObject())
		{
			DisplayName = Defaults->GetDisplayName();
			Icon = Defaults->GetIcon();
		}
	}

	OnNodeInitialized();
	RefreshNode();
}

void USkillTreeNodeWidget::RefreshNode()
{
	if (!IsValid(TreeComponent))
	{
		return;
	}
	State = TreeComponent->GetNodeState(NodeId);
	Rank = TreeComponent->GetNodeRank(NodeId);
	MaxRank = TreeComponent->GetNodeMaxRank(NodeId);

	OnNodeStateChanged(
		State,
		TreeComponent->CanUnlockNode(NodeId),
		TreeComponent->CanRefundNode(NodeId)
	);
}

bool USkillTreeNodeWidget::TryUnlock()
{
	return IsValid(TreeComponent) && TreeComponent->UnlockNode(NodeId);
	// Note: the tree widget listens to OnSkillTreeChanged and refreshes all
	// nodes, so we don't refresh here — one node unlocking can change the state
	// of its neighbours (Locked -> Available), so a tree-wide refresh is needed.
}

bool USkillTreeNodeWidget::TryRefund()
{
	return IsValid(TreeComponent) && TreeComponent->RefundNode(NodeId);
}


FReply USkillTreeNodeWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey Button = InMouseEvent.GetEffectingButton();

	if (Button == EKeys::LeftMouseButton)
	{
		if (InMouseEvent.IsShiftDown())
		{
			ToggleOnActionBar();
			return FReply::Handled();
		}

		// Placeable: wait to see whether this is a click (learn) or a drag (to the bar).
		if (CanPlaceOnActionBar())
		{
			bPendingClick = true;
			return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
		}

		TryUnlock();
		return FReply::Handled();
	}

	if (Button == EKeys::RightMouseButton)
	{
		TryRefund();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USkillTreeNodeWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPendingClick)
	{
		bPendingClick = false;
		TryUnlock();
		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void USkillTreeNodeWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	bPendingClick = false;

	if (!CanPlaceOnActionBar())
	{
		return;
	}

	UAbilityDragDropOperation* Operation = NewObject<UAbilityDragDropOperation>(this);
	Operation->AbilityClass = AbilityClass;
	Operation->SourceSlotIndex = INDEX_NONE;
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

UActionBarComponent* USkillTreeNodeWidget::FindActionBar() const
{
	return IsValid(TreeComponent) && IsValid(TreeComponent->GetOwner())
		? TreeComponent->GetOwner()->FindComponentByClass<UActionBarComponent>()
		: nullptr;
}

bool USkillTreeNodeWidget::CanPlaceOnActionBar() const
{
	const UActionBarComponent* ActionBar = FindActionBar();
	return IsValid(ActionBar) && ActionBar->CanPlaceAbility(AbilityClass);
}

bool USkillTreeNodeWidget::IsOnActionBar() const
{
	const UActionBarComponent* ActionBar = FindActionBar();
	return IsValid(ActionBar) && ActionBar->FindSlotOfAbility(AbilityClass) != INDEX_NONE;
}

bool USkillTreeNodeWidget::ToggleOnActionBar()
{
	UActionBarComponent* ActionBar = FindActionBar();
	return IsValid(ActionBar) && ActionBar->ToggleAbility(AbilityClass);
}

void USkillTreeNodeWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Super first: it runs the Blueprint On Mouse Enter, so hover visuals in the UMG graph still work.
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	OnNodeHovered.Broadcast(this);
}

void USkillTreeNodeWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	OnNodeUnhovered.Broadcast(this);
}