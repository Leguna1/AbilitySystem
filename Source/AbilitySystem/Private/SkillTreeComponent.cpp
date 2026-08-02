#include "SkillTreeComponent.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "SkillTreeAsset.h"

USkillTreeComponent::USkillTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	AbilityComponent = GetOwner()->FindComponentByClass<UAbilityComponent>();
	AvailableSkillPoints = FMath::Max(0, StartingSkillPoints);
}

void USkillTreeComponent::SetTreeAsset(USkillTreeAsset* InTreeAsset)
{
	TreeAsset = InTreeAsset;
	OnSkillTreeChanged.Broadcast();
}

void USkillTreeComponent::AddSkillPoints(const int32 Amount)
{
	if (Amount == 0)
	{
		return;
	}

	AvailableSkillPoints = FMath::Max(0, AvailableSkillPoints + Amount);
	OnSkillPointsChanged.Broadcast(AvailableSkillPoints);
	OnSkillTreeChanged.Broadcast();
}

int32 USkillTreeComponent::FindUnlockedIndex(const FGameplayTag NodeId) const
{
	for (int32 Index = 0; Index < UnlockedNodeIds.Num(); ++Index)
	{
		if (UnlockedNodeIds[Index].MatchesTagExact(NodeId))
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool USkillTreeComponent::IsNodeUnlocked(const FGameplayTag NodeId) const
{
	return FindUnlockedIndex(NodeId) != INDEX_NONE;
}

bool USkillTreeComponent::ArePrerequisitesMet(const FGameplayTag NodeId) const
{
	if (!IsValid(TreeAsset))
	{
		return false;
	}

	const FSkillTreeNode* Node = TreeAsset->FindNode(NodeId);
	if (!Node)
	{
		return false;
	}

	for (const FGameplayTag& Prereq : Node->Prerequisites)
	{
		if (!IsNodeUnlocked(Prereq))
		{
			return false;
		}
	}

	return true;
}

ESkillNodeState USkillTreeComponent::GetNodeState(const FGameplayTag NodeId) const
{
	if (IsNodeUnlocked(NodeId))
	{
		return ESkillNodeState::Unlocked;
	}

	return ArePrerequisitesMet(NodeId)
		? ESkillNodeState::Available
		: ESkillNodeState::Locked;
}

bool USkillTreeComponent::CanUnlockNode(const FGameplayTag NodeId) const
{
	if (!IsValid(TreeAsset))
	{
		return false;
	}

	const FSkillTreeNode* Node = TreeAsset->FindNode(NodeId);
	if (!Node)
	{
		return false;
	}

	if (IsNodeUnlocked(NodeId))
	{
		return false;
	}

	if (!ArePrerequisitesMet(NodeId))
	{
		return false;
	}

	return AvailableSkillPoints >= Node->Cost;
}

bool USkillTreeComponent::CanRefundNode(const FGameplayTag NodeId) const
{
	if (!IsValid(TreeAsset))
	{
		return false;
	}

	if (!IsNodeUnlocked(NodeId))
	{
		return false;
	}

	// Block refund while any dependent node is still unlocked (refund children first).
	TArray<FGameplayTag> Dependents;
	TreeAsset->GetDependentNodes(NodeId, Dependents);

	for (const FGameplayTag& Dependent : Dependents)
	{
		if (IsNodeUnlocked(Dependent))
		{
			return false;
		}
	}

	return true;
}

bool USkillTreeComponent::UnlockNode(const FGameplayTag NodeId)
{
	if (!CanUnlockNode(NodeId))
	{
		return false;
	}

	const FSkillTreeNode* Node = TreeAsset->FindNode(NodeId);
	if (!Node || !IsValid(Node->AbilityClass))
	{
		return false;
	}

	if (!IsValid(AbilityComponent) || !AbilityComponent->GrantAbility(Node->AbilityClass))
	{
		return false;
	}

	UnlockedNodeIds.Add(NodeId);
	AvailableSkillPoints -= Node->Cost;
	SpentSkillPoints += Node->Cost;

	OnSkillPointsChanged.Broadcast(AvailableSkillPoints);
	OnSkillTreeChanged.Broadcast();
	return true;
}

bool USkillTreeComponent::RefundNode(const FGameplayTag NodeId)
{
	if (!CanRefundNode(NodeId))
	{
		return false;
	}

	const FSkillTreeNode* Node = TreeAsset->FindNode(NodeId);
	if (!Node || !IsValid(Node->AbilityClass))
	{
		return false;
	}

	const UAbility* Defaults = Node->AbilityClass.GetDefaultObject();
	if (!IsValid(Defaults))
	{
		return false;
	}

	if (!IsValid(AbilityComponent) || !AbilityComponent->RemoveAbility(Defaults->GetAbilityId()))
	{
		return false;
	}

	UnlockedNodeIds.RemoveAt(FindUnlockedIndex(NodeId));
	AvailableSkillPoints += Node->Cost;
	SpentSkillPoints = FMath::Max(0, SpentSkillPoints - Node->Cost);

	OnSkillPointsChanged.Broadcast(AvailableSkillPoints);
	OnSkillTreeChanged.Broadcast();
	return true;
}