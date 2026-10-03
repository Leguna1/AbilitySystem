#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SkillTreeAsset.h"
#include "SkillTreeWidget.generated.h"

class UCanvasPanel;
class USizeBox;
class USkillTreeComponent;
class USkillTreeLinkWidget;
class USkillTreeNodeWidget;
class USkillTreeSectionWidget;

UENUM(BlueprintType)
enum class ESkillLinkState : uint8
{
	/** Prerequisite not learned yet. */
	Unmet UMETA(DisplayName = "Unmet"),

	/** Prerequisite learned: the node it leads to can be bought. */
	Met UMETA(DisplayName = "Met"),

	/** The node it leads to is learned. */
	Learned UMETA(DisplayName = "Learned"),

	/** The node it leads to is excluded by a choice in its group. */
	Excluded UMETA(DisplayName = "Excluded")
};

/** One prerequisite connection in canvas space, drawn as an elbow line. */
USTRUCT(BlueprintType)
struct FSkillTreeLink
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FGameplayTag PrerequisiteId;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FGameplayTag NodeId;

	/** Bottom center of the prerequisite node. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FVector2D From = FVector2D::ZeroVector;

	/** Top center of the dependent node. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	FVector2D To = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Tree")
	ESkillLinkState State = ESkillLinkState::Unmet;
};

/**
 * Lays out and displays a skill tree. Meant to be embedded in a page: it
 * reports its content size to an optional ContentSizeBox, so a ScrollBox or
 * ScaleBox around it can scroll or fit the tree.
 *
 * Layout: every node without prerequisites starts a column; children sit one
 * row below their first prerequisite, side by side; each column is as wide as
 * its widest row. Columns are grouped into framed sections.
 *
 * Required in the UMG subclass:
 *  - a Canvas Panel named "NodeCanvas";
 *  - NodeWidgetClass set.
 * Optional: "ContentSizeBox" (a SizeBox around NodeCanvas), SectionWidgetClass,
 * ExclusiveMarkerWidgetClass, LinkWidgetClass.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API USkillTreeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	void InitializeTree(USkillTreeComponent* InTreeComponent);

	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	void RebuildTree();

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	const TArray<FSkillTreeLink>& GetLinks() const { return Links; }

	/** Total pixel size of the laid-out tree, padding included. */
	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	FVector2D GetContentSize() const { return ContentSize; }

protected:
	virtual void NativeDestruct() override;

	/* -------------------- Bound widgets -------------------- */

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Skill Tree|UI")
	TObjectPtr<UCanvasPanel> NodeCanvas;

	/** Optional SizeBox wrapping NodeCanvas; receives the content size so parents can scroll or scale. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Tree|UI")
	TObjectPtr<USizeBox> ContentSizeBox;

	/* -------------------- Widget classes -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	TSubclassOf<USkillTreeNodeWidget> NodeWidgetClass;

	/** Background frame per section. Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	TSubclassOf<USkillTreeSectionWidget> SectionWidgetClass;

	/** Shown between neighbouring nodes of the same exclusive group (the "OR"). Optional. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	TSubclassOf<UUserWidget> ExclusiveMarkerWidgetClass;

	/** Line painter. Optional: the base class is used if unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	TSubclassOf<USkillTreeLinkWidget> LinkWidgetClass;

	/* -------------------- Layout -------------------- */

	/** Width reserved for a single node; a column is this times its widest row. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (ClampMin = "1.0"))
	float ColumnWidth = 120.0f;

	/** Vertical distance between rows (skill, path, upgrade...). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (ClampMin = "1.0"))
	float RowHeight = 140.0f;

	/** Gap between columns inside a section. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (ClampMin = "0.0"))
	float ColumnSpacing = 48.0f;

	/** Gap between sections. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (ClampMin = "0.0"))
	float SectionSpacing = 64.0f;

	/** Inner padding of a section frame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (ClampMin = "0.0"))
	float SectionPadding = 24.0f;

	/** Space at the top of each section for its title. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout", meta = (ClampMin = "0.0"))
	float SectionHeaderHeight = 48.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FVector2D ContentPadding = FVector2D(24.0f, 24.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FVector2D SkillNodeSize = FVector2D(96.0f, 96.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FVector2D PathNodeSize = FVector2D(72.0f, 72.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FVector2D UpgradeNodeSize = FVector2D(56.0f, 56.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FVector2D PassiveNodeSize = FVector2D(72.0f, 72.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Layout")
	FVector2D ExclusiveMarkerSize = FVector2D(40.0f, 24.0f);

	/* -------------------- Link style -------------------- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Links")
	FLinearColor UnmetLinkColor = FLinearColor(0.25f, 0.25f, 0.28f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Links")
	FLinearColor MetLinkColor = FLinearColor(0.6f, 0.6f, 0.65f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Links")
	FLinearColor LearnedLinkColor = FLinearColor(0.95f, 0.78f, 0.35f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Links")
	FLinearColor ExcludedLinkColor = FLinearColor(0.45f, 0.12f, 0.12f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|Links", meta = (ClampMin = "0.5"))
	float LinkThickness = 3.0f;

private:
	struct FSectionLayout
	{
		FName SectionId;
		FText DisplayName;
		FVector2D Position = FVector2D::ZeroVector;
		FVector2D Size = FVector2D::ZeroVector;
	};

	UFUNCTION()
	void HandleTreeChanged();

	void ClearTree();
	void ComputeLayout(const USkillTreeAsset& Tree);
	float MeasureSubtree(int32 NodeIndex, int32 Depth);
	void PlaceSubtree(int32 NodeIndex, float Left, int32 Depth);
	float GetRowCenterY(int32 Depth) const;
	FVector2D GetNodeSize(ESkillNodeKind Kind) const;

	void BuildSections();
	void BuildLinkLayer();
	void BuildMarkers();
	void BuildNodes(const USkillTreeAsset& Tree);
	void BuildLinks(const USkillTreeAsset& Tree);
	void ApplyContentSize() const;

	void RefreshAllNodes();
	void RefreshLinkStates();
	ESkillLinkState ResolveLinkState(const FSkillTreeLink& Link) const;

	UPROPERTY(Transient)
	TObjectPtr<USkillTreeComponent> TreeComponent;

	UPROPERTY(Transient)
	TObjectPtr<USkillTreeLinkWidget> LinkWidget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkillTreeNodeWidget>> NodeWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkillTreeSectionWidget>> SectionWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidget>> MarkerWidgets;

	UPROPERTY(Transient)
	TArray<FSkillTreeLink> Links;

	/* Layout results, parallel to the tree asset's Nodes array. */
	TArray<FVector2D> NodeCenters;
	TArray<bool> NodePlaced;
	TArray<TArray<int32>> LayoutChildren;
	TArray<float> SubtreeWidths;

	TArray<FSectionLayout> SectionLayouts;
	TArray<FVector2D> MarkerCenters;

	FVector2D ContentSize = FVector2D::ZeroVector;
	int32 MaxDepth = 0;
};