#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SkillTreeAsset.generated.h"

class UAbility;

/** What a node represents. Drives its size in the layout and its look in Blueprint. */
UENUM(BlueprintType)
enum class ESkillNodeKind : uint8
{
	Skill UMETA(DisplayName = "Skill"),
	Path UMETA(DisplayName = "Path"),
	Upgrade UMETA(DisplayName = "Upgrade"),
	Passive UMETA(DisplayName = "Passive")
};

/** A framed area of the tree (e.g. Basic, Special). Sections are laid out left to right in asset order. */
USTRUCT(BlueprintType)
struct FSkillTreeSection
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	FName SectionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	FText DisplayName;
};

/** One node: an ability the player can learn, its cost, what it depends on, and how it is shown. */
USTRUCT(BlueprintType)
struct FSkillTreeNode
{
	GENERATED_BODY()

	/** Unique identity of this node within the tree. Prerequisites reference this. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	FGameplayTag NodeId;

	/** Ability whose rank this node raises. Passives are also granted by the tree. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TSubclassOf<UAbility> AbilityClass;

	/** Skill points per rank. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree", meta = (ClampMin = "0"))
	int32 Cost = 1;

	/** Node ids that must ALL be unlocked first. The first one is also this node's parent in the layout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TArray<FGameplayTag> Prerequisites;

	/** Nodes sharing a group are mutually exclusive. None = no group. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	FName ExclusiveGroup = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	ESkillNodeKind Kind = ESkillNodeKind::Skill;

	/** Section of this node's column. Only read on column roots (no prerequisites); children follow their parent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FName Section = NAME_None;

	/** Skip the auto-layout for this node and place its center at CanvasPosition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	bool bUseManualPosition = false;

	/** Center of the node in tree pixels, used only with bUseManualPosition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (EditCondition = "bUseManualPosition"))
	FVector2D CanvasPosition = FVector2D::ZeroVector;
};

/**
 * Designer-authored definition of a skill tree. Pure data: nodes, costs,
 * prerequisites and presentation hints. Runtime state lives on
 * USkillTreeComponent; positions come from USkillTreeWidget's auto-layout.
 */
UCLASS(BlueprintType)
class ABILITYSYSTEM_API USkillTreeAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Framed areas, left to right. Columns whose Section isn't listed are placed after these. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	TArray<FSkillTreeSection> Sections;

	/** Nodes. Within a section, columns follow this order; siblings follow it too. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TArray<FSkillTreeNode> Nodes;

	const FSkillTreeNode* FindNode(FGameplayTag NodeId) const;
	void GetDependentNodes(FGameplayTag NodeId, TArray<FGameplayTag>& OutDependents) const;
};