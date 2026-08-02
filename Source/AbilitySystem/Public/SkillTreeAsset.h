#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SkillTreeAsset.generated.h"

class UAbility;

/** One node in the skill tree: an ability the player can unlock, its cost, and what it depends on. */
USTRUCT(BlueprintType)
struct FSkillTreeNode
{
	GENERATED_BODY()

	/** Unique identity of this node within the tree. Prerequisites reference this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	FGameplayTag NodeId;

	/** Ability granted when this node is unlocked. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TSubclassOf<UAbility> AbilityClass;

	/** Skill points required to unlock this node. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree", meta = (ClampMin = "0"))
	int32 Cost = 1;

	/** Node ids that must ALL be unlocked before this node becomes available. Empty = a root node. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TArray<FGameplayTag> Prerequisites;

	/** Editor-only layout hint, in abstract grid/canvas units. The widget maps this to screen space. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FVector2D CanvasPosition = FVector2D::ZeroVector;
};

/**
 * Designer-authored definition of a skill tree. Pure data: the list of nodes,
 * their costs, and the prerequisite links between them. Holds no runtime state
 * (what is unlocked lives on USkillTreeComponent).
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API USkillTreeAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TArray<FSkillTreeNode> Nodes;

	/** Finds a node by id. Returns nullptr if not present. */
	const FSkillTreeNode* FindNode(FGameplayTag NodeId) const;

	/** Returns the ids of every node that lists NodeId among its prerequisites. */
	void GetDependentNodes(FGameplayTag NodeId, TArray<FGameplayTag>& OutDependents) const;
};