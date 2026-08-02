#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SkillTreeComponent.generated.h"

class UAbilityComponent;
class USkillTreeAsset;

/** Derived display state of a node, computed from the tree data + what is unlocked. */
UENUM(BlueprintType)
enum class ESkillNodeState : uint8
{
	/** Prerequisites not all met. Cannot be unlocked yet. */
	Locked UMETA(DisplayName = "Locked"),

	/** Prerequisites met but not yet unlocked. Buyable if points allow. */
	Available UMETA(DisplayName = "Available"),

	/** Already unlocked. */
	Unlocked UMETA(DisplayName = "Unlocked")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSkillTreeChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSkillPointsChangedSignature, int32, AvailablePoints);

/**
 * Runtime state and rules for a skill tree: how many points the player has,
 * which nodes are unlocked, and the unlock/refund operations. Reads its shape
 * from a USkillTreeAsset and drives grants through the UAbilityComponent.
 *
 * Refund policy: a node cannot be refunded while any node depending on it is
 * still unlocked (refund children first). See CanRefundNode.
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
	int32 GetAvailableSkillPoints() const { return AvailableSkillPoints; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|Points")
	int32 GetSpentSkillPoints() const { return SpentSkillPoints; }

	/* -------------------- Queries -------------------- */

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool IsNodeUnlocked(FGameplayTag NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	ESkillNodeState GetNodeState(FGameplayTag NodeId) const;

	/** True if prerequisites are all met (regardless of points). */
	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool ArePrerequisitesMet(FGameplayTag NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool CanUnlockNode(FGameplayTag NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree")
	bool CanRefundNode(FGameplayTag NodeId) const;

	/* -------------------- Operations -------------------- */

	/** Spends points and grants the ability. Returns false if not currently unlockable. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	bool UnlockNode(FGameplayTag NodeId);

	/** Refunds points and removes the ability. Returns false if not currently refundable. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree")
	bool RefundNode(FGameplayTag NodeId);

	/* -------------------- Events -------------------- */

	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|Events")
	FSkillTreeChangedSignature OnSkillTreeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|Events")
	FSkillPointsChangedSignature OnSkillPointsChanged;

protected:
	virtual void BeginPlay() override;

	/** Ability component grants/removes are routed through. Found on the owner at BeginPlay. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Skill Tree", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilityComponent> AbilityComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TObjectPtr<USkillTreeAsset> TreeAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Points", meta = (ClampMin = "0"))
	int32 StartingSkillPoints = 0;

private:
	int32 FindUnlockedIndex(FGameplayTag NodeId) const;

	UPROPERTY(Transient)
	TArray<FGameplayTag> UnlockedNodeIds;

	UPROPERTY(Transient)
	int32 AvailableSkillPoints = 0;

	UPROPERTY(Transient)
	int32 SpentSkillPoints = 0;
};