#include "SkillTreeAsset.h"

#include "Ability.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

int32 FSkillTreeNode::GetRequiredLevelForRank(const int32 ForRank) const
{
	if (RequiredLevelPerRank.IsEmpty() || ForRank <= 0)
	{
		return 0;
	}

	// Ranks past the end of the list keep the last requirement.
	const int32 Index = FMath::Min(ForRank, RequiredLevelPerRank.Num()) - 1;
	return FMath::Max(0, RequiredLevelPerRank[Index]);
}

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
#if WITH_EDITOR
EDataValidationResult USkillTreeAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const FSkillTreeNode& Node : Nodes)
	{
		if (Node.RequiredLevelPerRank.IsEmpty())
		{
			continue;
		}

		const FText NodeName = FText::FromString(Node.NodeId.ToString());
		const UAbility* Defaults = Node.AbilityClass ? Node.AbilityClass.GetDefaultObject() : nullptr;

		if (IsValid(Defaults) && Node.RequiredLevelPerRank.Num() > Defaults->GetMaxRank())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("SkillTree", "LevelListTooLong", "{0}: Required Level Per Rank has {1} entries but the ability's Max Rank is {2}. The extra entries are never used."),
				NodeName, Node.RequiredLevelPerRank.Num(), Defaults->GetMaxRank()));
		}

		for (int32 Index = 1; Index < Node.RequiredLevelPerRank.Num(); ++Index)
		{
			if (Node.RequiredLevelPerRank[Index] < Node.RequiredLevelPerRank[Index - 1])
			{
				Context.AddWarning(FText::Format(
					NSLOCTEXT("SkillTree", "LevelListDecreasing", "{0}: rank {1} needs a lower level than rank {2}."),
					NodeName, Index + 1, Index));
				break;
			}
		}
	}

	return Result;
}
#endif
