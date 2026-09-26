#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SkillTreeComponent.generated.h"

class UAbilityComponent;
class USkillTreeAsset;
struct FSkillTreeNode;

/** Derived display state of a node, computed from tree data + ability ranks. */
UENUM(BlueprintType)
enum class ESkillNodeState : uint8
{
	/** Rank 0 and prerequisites not all met. */
	Locked UMETA(DisplayName = "Locked"),

	/** Rank 0, prerequisites met. Buyable if points allow. */
	Available UMETA(DisplayName = "Available"),

	/** Rank 1+ and below max. Upgradable if points allow. */
	Unlocked UMETA(DisplayName = "Unlocked"),

	/** At max rank. */
	Maxed UMETA(DisplayName = "Maxed")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSkillTreeChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSkillPointsChangedSignature, int32, AvailablePoints);

/**
 * Progression rules for a skill tree. Owns only the points earned; learned
 * ranks live on UAbilityComponent (single source of truth), and spent points
 * are derived from them.
 *
 * Each node advances one ability by one rank per purchase. Refund policy: the
 * rank that would return an ability to 0 cannot be refunded while any
 * dependent node has purchased ranks.
 */
UCLASS(ClassGroup = (Ability), meta = (BlueprintSpawnableComponent))
class ABILITYSYSTEM_API USkillTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillTreeComponent();

	/* -------------------- Setup -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	void SetTreeAsset(USkillTreeAsset* InTreeAsset);

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	USkillTreeAsset* GetTreeAsset() const { return TreeAsset; }

	/* -------------------- Points -------------------- */

	UFUNCTION(BlueprintCallable, Category = "Skill Tree|Points")
	void AddSkillPoints(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Skill Tree|Points")
	int32 GetTotalSkillPoints() const { return TotalSkillPoints; }

	/** Sum of learned ranks x node cost across the tree. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|Points")
	int32 GetSpentSkillPoints() const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree|Points")
	int32 GetAvailableSkillPoints() const;

	/* -------------------- Queries -------------------- */

	/** Effective rank of the node's ability (starting + learned). */
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	int32 GetNodeRank(FGameplayTag NodeId) const;

	/** Ranks bought through the tree (excludes free starting ranks). */
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	int32 GetNodeLearnedRank(FGameplayTag NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	int32 GetNodeMaxRank(FGameplayTag NodeId) const;

	/** True when the node's ability is usable (rank 1+). */
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool IsNodeUnlocked(FGameplayTag NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	ESkillNodeState GetNodeState(FGameplayTag NodeId) const;

	/** True if every prerequisite's ability is rank 1+ (regardless of points). */
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool ArePrerequisitesMet(FGameplayTag NodeId) const;

	/** Whether one more rank can be bought right now. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool CanUnlockNode(FGameplayTag NodeId) const;

	/** Whether one rank can be refunded right now. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool CanRefundNode(FGameplayTag NodeId) const;

	/* -------------------- Operations -------------------- */

	/** Buys one rank. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	bool UnlockNode(FGameplayTag NodeId);

	/** Refunds one rank. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	bool RefundNode(FGameplayTag NodeId);

	/* -------------------- Events -------------------- */

	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|Events")
	FSkillTreeChangedSignature OnSkillTreeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|Events")
	FSkillPointsChangedSignature OnSkillPointsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Ranks are read from and written to this component. Found on the owner at BeginPlay. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Skill Tree")
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TObjectPtr<USkillTreeAsset> TreeAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Points", meta = (ClampMin = "0"))
	int32 StartingSkillPoints = 0;

private:
	/** The node, if it exists and has an ability class. */
	const FSkillTreeNode* FindValidNode(FGameplayTag NodeId) const;

	UFUNCTION()
	void HandleAbilityRankChanged(FGameplayTag AbilityId, int32 OldRank, int32 NewRank);

	void BroadcastTreeChanged();

	/** Points earned over the whole game. Available = Total - Spent. */
	UPROPERTY(Transient)
	int32 TotalSkillPoints = 0;
};