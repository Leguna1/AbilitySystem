#include "SkillTreeAsset.h"

const FSkillTreeNode* USkillTreeAsset::FindNode(const FGameplayTag NodeId) const
{
	if (!NodeId.IsValid())
	{
		return nullptr;
	}

	for (const FSkillTreeNode& Node : Nodes)
	{
		if (Node.NodeId.MatchesTagExact(NodeId))
		{
			return &Node;
		}
	}

	return nullptr;
}

void USkillTreeAsset::GetDependentNodes(const FGameplayTag NodeId, TArray<FGameplayTag>& OutDependents) const
{
	OutDependents.Reset();

	if (!NodeId.IsValid())
	{
		return;
	}

	for (const FSkillTreeNode& Node : Nodes)
	{
		for (const FGameplayTag& Prereq : Node.Prerequisites)
		{
			if (Prereq.MatchesTagExact(NodeId))
			{
				OutDependents.Add(Node.NodeId);
				break;
			}
		}
	}
}