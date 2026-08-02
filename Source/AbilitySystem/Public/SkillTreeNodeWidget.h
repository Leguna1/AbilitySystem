#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SkillTreeComponent.h"
#include "SkillTreeNodeWidget.generated.h"

class UAbility;
class USkillTreeComponent;
class UTexture2D;

/**
 * One node in the skill tree. Reads its display data from the granted ability's
 * defaults and its lock state from the SkillTreeComponent.
 *
 * The C++ base owns the data; the UMG subclass owns the look. Implement the
 * BlueprintImplementableEvents to reflect state, and call TryUnlock/TryRefund
 * from your click handlers.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API USkillTreeNodeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds this node widget to a node id in the tree. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	void InitializeNode(USkillTreeComponent* InTreeComponent, FGameplayTag InNodeId);

	/** Re-reads state from the component and pushes it to the visuals. Called by the tree on any change. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	void RefreshNode();

	/** Attempt to buy this node. Wire to your click / buy button. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	bool TryUnlock();

	/** Attempt to refund this node. Wire to right-click / refund button. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	bool TryRefund();

	/* -------------------- Getters for the UMG graph -------------------- */

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	FGameplayTag GetNodeId() const { return NodeId; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	UTexture2D* GetIcon() const { return Icon; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	int32 GetCost() const { return Cost; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	ESkillNodeState GetState() const { return State; }

	/** Canvas position authored on the node, for the tree to place this widget. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	FVector2D GetCanvasPosition() const { return CanvasPosition; }

protected:
	/** Populate static visuals (icon, name, cost) here. Called once after init. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Tree|UI")
	void OnNodeInitialized();

	/**
	 * Reflect the node's state here: grey out Locked, highlight Available,
	 * mark Unlocked. Called on init and on every refresh.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Tree|UI")
	void OnNodeStateChanged(ESkillNodeState NewState, bool bCanUnlockNow, bool bCanRefundNow);

private:
	UPROPERTY(Transient)
	TObjectPtr<USkillTreeComponent> TreeComponent;

	UPROPERTY(Transient)
	FGameplayTag NodeId;

	UPROPERTY(Transient)
	FText DisplayName;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(Transient)
	int32 Cost = 0;

	UPROPERTY(Transient)
	FVector2D CanvasPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	ESkillNodeState State = ESkillNodeState::Locked;
};