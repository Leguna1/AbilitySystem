#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SkillTreeWidget.generated.h"

class UCanvasPanel;
class USkillTreeComponent;
class USkillTreeNodeWidget;

/** A single prereq connection, in local canvas space, for OnPaint to draw. */
USTRUCT(BlueprintType)
struct FSkillTreeLink
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FVector2D From = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FVector2D To = FVector2D::ZeroVector;

	/** True when the prerequisite (To) is unlocked - lets you color satisfied links. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	bool bPrerequisiteMet = false;
};

/**
 * The tree canvas. Spawns one node widget per node at its authored position,
 * builds the list of prerequisite links, and refreshes every node whenever the
 * tree state changes.
 *
 * Two UMG requirements in the subclass:
 *  - a Canvas Panel named "NodeCanvas" (BindWidget) to hold the nodes;
 *  - set NodeWidgetClass to your USkillTreeNodeWidget subclass.
 *
 * Line drawing: the base exposes GetLinks(); implement drawing either in an
 * OnPaint override or a Blueprint that iterates GetLinks and draws each.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API USkillTreeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Binds to the component and builds the whole tree. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	void InitializeTree(USkillTreeComponent* InTreeComponent);

	/** Clears and rebuilds all node widgets + links from the current tree asset. */
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	void RebuildTree();

	/** Prerequisite links in canvas space, for line drawing. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	const TArray<FSkillTreeLink>& GetLinks() const { return Links; }

	/** Scales authored CanvasPosition units to slate pixels. Tune per tree density. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	float PositionScale = 1.0f;

	/** Size of each node widget on the canvas, in pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	FVector2D NodeSize = FVector2D(96.0f, 96.0f);

protected:
	virtual void NativeDestruct() override;

	/** Canvas panel that node widgets are added to. Bind a panel named "NodeCanvas". */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Tree|UI")
	TObjectPtr<UCanvasPanel> NodeCanvas;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	TSubclassOf<USkillTreeNodeWidget> NodeWidgetClass;

	/** Widget that paints the prereq lines, added behind all nodes. Optional: a
	 *  default is created if left unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	TSubclassOf<class USkillTreeLinkWidget> LinkWidgetClass;

	/** Color for a link whose prerequisite is met. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	FLinearColor MetLinkColor = FLinearColor(0.9f, 0.75f, 0.35f, 1.0f);

	/** Color for a link whose prerequisite is not yet met. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	FLinearColor UnmetLinkColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	float LinkThickness = 2.0f;

private:
	UFUNCTION()
	void HandleTreeChanged();

	void ClearNodes();
	void BuildNodes();
	void BuildLinks();
	void RefreshAllNodes();

	/** Center point of a node in canvas-local space, from its authored position. */
	FVector2D NodeCenter(const FVector2D& AuthoredPosition) const;

	UPROPERTY(Transient)
	TObjectPtr<USkillTreeComponent> TreeComponent;

	UPROPERTY(Transient)
	TObjectPtr<class USkillTreeLinkWidget> LinkWidget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkillTreeNodeWidget>> NodeWidgets;

	UPROPERTY(Transient)
	TArray<FSkillTreeLink> Links;
};