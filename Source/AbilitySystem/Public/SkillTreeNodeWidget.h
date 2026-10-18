#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SkillTreeComponent.h"
#include "SkillTreeAsset.h"
#include "SkillTreeNodeWidget.generated.h"

class UAbility;
class UActionBarComponent;
class USkillTreeComponent;
class USkillTreeNodeWidget;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSkillTreeNodeHoverSignature, USkillTreeNodeWidget*, NodeWidget);

/**
 * One node in the skill tree. Reads its display data from the granted ability's
 * defaults and its lock state from the SkillTreeComponent.
 *
 * The C++ base owns the data; the UMG subclass owns the look. Implement the
 * BlueprintImplementableEvents to reflect state.
 *
 * Mouse: left-click learns, right-click refunds, Shift + left-click adds or removes it on the action bar,
 * and a learned active ability can be dragged onto an action bar slot.
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

	/** Adds this node's ability to the first empty action bar slot, or removes it if it's there. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	bool ToggleOnActionBar();

	/** Learned, an active ability, and meant for the action bar. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	bool CanPlaceOnActionBar() const;

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	bool IsOnActionBar() const;

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
	
	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	int32 GetRank() const { return Rank; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	int32 GetMaxRank() const { return MaxRank; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	ESkillNodeKind GetKind() const { return Kind; }

	/** Level needed for the next rank. 0 = no requirement (or maxed). */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	int32 GetRequiredLevel() const { return RequiredLevel; }

	/** False while the character is below Get Required Level: show a "Lv X" badge, tint it red, etc. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	bool IsLevelRequirementMet() const { return bLevelRequirementMet; }

	/** The mouse moved onto this node. The tree uses it to fill its details panel. */
	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|UI")
	FSkillTreeNodeHoverSignature OnNodeHovered;

	/** The mouse left this node. */
	UPROPERTY(BlueprintAssignable, Category = "Skill Tree|UI")
	FSkillTreeNodeHoverSignature OnNodeUnhovered;

protected:
	/** Populate static visuals (icon, name, cost) here. Called once after init. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Tree|UI")
	void OnNodeInitialized();

	/**
	 * Reflect the node's state here: grey out Locked, highlight Available,
	 * mark Unlocked, show the level on Level Locked. Called on init and on every refresh.
	 * Get Required Level / Is Level Requirement Met are already up to date when this runs.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Tree|UI")
	void OnNodeStateChanged(ESkillNodeState NewState, bool bCanUnlockNow, bool bCanRefundNow);
	
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	/** The widget that follows the cursor while dragging. Null = the icon. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Tree|UI")
	UWidget* BuildDragVisual();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Tree|UI")
	FVector2D DragIconSize = FVector2D(48.0f, 48.0f);
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	UActionBarComponent* FindActionBar() const;

	UPROPERTY(Transient)
	TObjectPtr<USkillTreeComponent> TreeComponent;

	UPROPERTY(Transient)
	TSubclassOf<UAbility> AbilityClass;

	/** A left press that may still become a click (learn) rather than a drag. */
	bool bPendingClick = false;

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
	
	UPROPERTY(Transient)
	int32 Rank = 0;

	UPROPERTY(Transient)
	int32 MaxRank = 0;
	
	UPROPERTY(Transient)
	ESkillNodeKind Kind = ESkillNodeKind::Skill;

	UPROPERTY(Transient)
	int32 RequiredLevel = 0;

	UPROPERTY(Transient)
	bool bLevelRequirementMet = true;
};