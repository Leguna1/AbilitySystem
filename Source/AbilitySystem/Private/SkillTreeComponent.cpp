#include "SkillTreeComponent.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "PassiveAbility.h"
#include "SkillTreeAsset.h"

USkillTreeComponent::USkillTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	TotalSkillPoints = FMath::Max(0, StartingSkillPoints);
	AbilityComponent = GetOwner()->FindComponentByClass<UAbilityComponent>();

	if (!IsValid(AbilityComponent))
	{
		UE_LOG(LogTemp, Warning, TEXT("USkillTreeComponent on %s found no UAbilityComponent."), *GetNameSafe(GetOwner()));
		return;
	}

	// Every rank change (purchase, refund, save load, debug) refreshes the tree through one path.
	AbilityComponent->AbilityRankChangedEvent.AddDynamic(this, &USkillTreeComponent::HandleAbilityRankChanged);
	
	GrantTreePassives();
}

void USkillTreeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(AbilityComponent))
	{
		RevokeTreePassives();
		AbilityComponent->AbilityRankChangedEvent.RemoveDynamic(this, &USkillTreeComponent::HandleAbilityRankChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void USkillTreeComponent::SetTreeAsset(USkillTreeAsset* InTreeAsset)
{
	RevokeTreePassives();

	TreeAsset = InTreeAsset;

	GrantTreePassives();
	BroadcastTreeChanged();
}

void USkillTreeComponent::AddSkillPoints(const int32 Amount)
{
	if (Amount == 0)
	{
		return;
	}

	TotalSkillPoints = FMath::Max(0, TotalSkillPoints + Amount);
	BroadcastTreeChanged();
}

int32 USkillTreeComponent::GetSpentSkillPoints() const
{
	if (!IsValid(TreeAsset) || !IsValid(AbilityComponent))
	{
		return 0;
	}

	int32 Spent = 0;

	for (const FSkillTreeNode& Node : TreeAsset->Nodes)
	{
		if (Node.AbilityClass)
		{
			Spent += AbilityComponent->GetLearnedRank(Node.AbilityClass) * Node.Cost;
		}
	}

	return Spent;
}

int32 USkillTreeComponent::GetAvailableSkillPoints() const
{
	return FMath::Max(0, TotalSkillPoints - GetSpentSkillPoints());
}

const FSkillTreeNode* USkillTreeComponent::FindValidNode(const FGameplayTag NodeId) const
{
	if (!IsValid(TreeAsset))
	{
		return nullptr;
	}

	const FSkillTreeNode* Node = TreeAsset->FindNode(NodeId);
	return Node && Node->AbilityClass ? Node : nullptr;
}

int32 USkillTreeComponent::GetNodeRank(const FGameplayTag NodeId) const
{
	const FSkillTreeNode* Node = FindValidNode(NodeId);

	return Node && IsValid(AbilityComponent)
		? AbilityComponent->GetAbilityRank(Node->AbilityClass)
		: 0;
}

int32 USkillTreeComponent::GetNodeLearnedRank(const FGameplayTag NodeId) const
{
	const FSkillTreeNode* Node = FindValidNode(NodeId);

	return Node && IsValid(AbilityComponent)
		? AbilityComponent->GetLearnedRank(Node->AbilityClass)
		: 0;
}

int32 USkillTreeComponent::GetNodeMaxRank(const FGameplayTag NodeId) const
{
	const FSkillTreeNode* Node = FindValidNode(NodeId);
	const UAbility* Defaults = Node ? Node->AbilityClass.GetDefaultObject() : nullptr;

	return IsValid(Defaults) ? Defaults->GetMaxRank() : 0;
}

bool USkillTreeComponent::IsNodeUnlocked(const FGameplayTag NodeId) const
{
	return GetNodeRank(NodeId) > 0;
}

ESkillNodeState USkillTreeComponent::GetNodeState(const FGameplayTag NodeId) const
{
	const int32 Rank = GetNodeRank(NodeId);

	if (Rank > 0)
	{
		return Rank >= GetNodeMaxRank(NodeId)
			? ESkillNodeState::Maxed
			: ESkillNodeState::Unlocked;
	}

	if (IsNodeExcluded(NodeId))
	{
		return ESkillNodeState::Excluded;
	}

	return ArePrerequisitesMet(NodeId)
		? ESkillNodeState::Available
		: ESkillNodeState::Locked;
}

bool USkillTreeComponent::ArePrerequisitesMet(const FGameplayTag NodeId) const
{
	const FSkillTreeNode* Node = FindValidNode(NodeId);

	if (!Node)
	{
		return false;
	}

	for (const FGameplayTag& Prerequisite : Node->Prerequisites)
	{
		if (!IsNodeUnlocked(Prerequisite))
		{
			return false;
		}
	}

	return true;
}

bool USkillTreeComponent::CanUnlockNode(const FGameplayTag NodeId) const
{
	const FSkillTreeNode* Node = FindValidNode(NodeId);

	return Node &&
	IsValid(AbilityComponent) &&
	!IsNodeExcluded(NodeId) &&
	ArePrerequisitesMet(NodeId) &&
	GetNodeRank(NodeId) < GetNodeMaxRank(NodeId) &&
	GetAvailableSkillPoints() >= Node->Cost;
}

bool USkillTreeComponent::CanRefundNode(const FGameplayTag NodeId) const
{
	if (!FindValidNode(NodeId) || !IsValid(AbilityComponent) || GetNodeLearnedRank(NodeId) <= 0)
	{
		return false;
	}

	// Only the rank that returns the ability to 0 can strand dependents.
	if (GetNodeRank(NodeId) > 1)
	{
		return true;
	}

	TArray<FGameplayTag> Dependents;
	TreeAsset->GetDependentNodes(NodeId, Dependents);

	for (const FGameplayTag& Dependent : Dependents)
	{
		if (GetNodeLearnedRank(Dependent) > 0)
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

	const FSkillTreeNode* Node = FindValidNode(NodeId);

	// Tree and points events follow from AbilityRankChangedEvent.
	AbilityComponent->SetLearnedRank(Node->AbilityClass, AbilityComponent->GetLearnedRank(Node->AbilityClass) + 1);
	return true;
}

bool USkillTreeComponent::RefundNode(const FGameplayTag NodeId)
{
	if (!CanRefundNode(NodeId))
	{
		return false;
	}

	const FSkillTreeNode* Node = FindValidNode(NodeId);

	AbilityComponent->SetLearnedRank(Node->AbilityClass, AbilityComponent->GetLearnedRank(Node->AbilityClass) - 1);
	return true;
}

void USkillTreeComponent::HandleAbilityRankChanged(const FGameplayTag AbilityId, const int32 OldRank, const int32 NewRank)
{
	BroadcastTreeChanged();
}

void USkillTreeComponent::BroadcastTreeChanged()
{
	OnSkillPointsChanged.Broadcast(GetAvailableSkillPoints());
	OnSkillTreeChanged.Broadcast();
}
bool USkillTreeComponent::IsNodeExcluded(const FGameplayTag NodeId) const
{
	const FSkillTreeNode* Node = FindValidNode(NodeId);

	// A node that already has bought ranks is the one doing the excluding.
	if (!Node || Node->ExclusiveGroup.IsNone() || GetNodeLearnedRank(NodeId) > 0)
	{
		return false;
	}

	for (const FSkillTreeNode& Other : TreeAsset->Nodes)
	{
		if (Other.ExclusiveGroup == Node->ExclusiveGroup &&
			!Other.NodeId.MatchesTagExact(NodeId) &&
			GetNodeLearnedRank(Other.NodeId) > 0)
		{
			return true;
		}
	}

	return false;
}

void USkillTreeComponent::GrantTreePassives()
{
	if (!IsValid(AbilityComponent) || !IsValid(TreeAsset))
	{
		return;
	}

	TArray<TSubclassOf<UAbility>> Passives;

	for (const FSkillTreeNode& Node : TreeAsset->Nodes)
	{
		if (Node.AbilityClass && Node.AbilityClass->IsChildOf(UPassiveAbility::StaticClass()))
		{
			Passives.AddUnique(Node.AbilityClass);
		}
	}

	AbilityComponent->GrantAbilities(Passives, this);
}

void USkillTreeComponent::RevokeTreePassives()
{
	if (IsValid(AbilityComponent))
	{
		AbilityComponent->RevokeAbilitiesFromSource(this);
	}
}