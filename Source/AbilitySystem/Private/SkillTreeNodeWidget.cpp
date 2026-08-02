#include "SkillTreeNodeWidget.h"

#include "Ability.h"
#include "SkillTreeAsset.h"

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