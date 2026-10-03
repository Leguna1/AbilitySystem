#include "SkillTreeWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "SkillTreeComponent.h"
#include "SkillTreeLinkWidget.h"
#include "SkillTreeNodeWidget.h"
#include "SkillTreeSectionWidget.h"

namespace
{
	int32 FindNodeIndex(const USkillTreeAsset& Tree, const FGameplayTag NodeId)
	{
		for (int32 Index = 0; Index < Tree.Nodes.Num(); ++Index)
		{
			if (Tree.Nodes[Index].NodeId.IsValid() && Tree.Nodes[Index].NodeId.MatchesTagExact(NodeId))
			{
				return Index;
			}
		}

		return INDEX_NONE;
	}

	FText FindSectionName(const USkillTreeAsset& Tree, const FName SectionId)
	{
		for (const FSkillTreeSection& Section : Tree.Sections)
		{
			if (Section.SectionId == SectionId)
			{
				return Section.DisplayName;
			}
		}

		return SectionId.IsNone() ? FText::GetEmpty() : FText::FromName(SectionId);
	}
}

void USkillTreeWidget::InitializeTree(USkillTreeComponent* InTreeComponent)
{
	if (IsValid(TreeComponent))
	{
		TreeComponent->OnSkillTreeChanged.RemoveDynamic(this, &USkillTreeWidget::HandleTreeChanged);
	}

	TreeComponent = InTreeComponent;

	if (IsValid(TreeComponent))
	{
		TreeComponent->OnSkillTreeChanged.AddDynamic(this, &USkillTreeWidget::HandleTreeChanged);
	}

	RebuildTree();
}

void USkillTreeWidget::RebuildTree()
{
	ClearTree();

	const USkillTreeAsset* Tree = IsValid(TreeComponent) ? TreeComponent->GetTreeAsset() : nullptr;

	if (!IsValid(Tree) || !IsValid(NodeCanvas) || !IsValid(NodeWidgetClass))
	{
		return;
	}

	ComputeLayout(*Tree);

	// Add order is paint order: frames, lines, "OR" markers, then nodes on top.
	BuildSections();
	BuildLinkLayer();
	BuildMarkers();
	BuildNodes(*Tree);
	BuildLinks(*Tree);

	ApplyContentSize();
	RefreshAllNodes();
}

void USkillTreeWidget::NativeDestruct()
{
	if (IsValid(TreeComponent))
	{
		TreeComponent->OnSkillTreeChanged.RemoveDynamic(this, &USkillTreeWidget::HandleTreeChanged);
	}

	Super::NativeDestruct();
}

void USkillTreeWidget::HandleTreeChanged()
{
	RefreshAllNodes();
}

void USkillTreeWidget::ClearTree()
{
	if (IsValid(NodeCanvas))
	{
		NodeCanvas->ClearChildren();
	}

	LinkWidget = nullptr;
	NodeWidgets.Reset();
	SectionWidgets.Reset();
	MarkerWidgets.Reset();
	Links.Reset();

	NodeCenters.Reset();
	NodePlaced.Reset();
	LayoutChildren.Reset();
	SubtreeWidths.Reset();
	SectionLayouts.Reset();
	MarkerCenters.Reset();

	ContentSize = FVector2D::ZeroVector;
	MaxDepth = 0;
}

/* -------------------- Layout -------------------- */

void USkillTreeWidget::ComputeLayout(const USkillTreeAsset& Tree)
{
	const int32 NodeCount = Tree.Nodes.Num();

	NodeCenters.Init(FVector2D::ZeroVector, NodeCount);
	NodePlaced.Init(false, NodeCount);
	SubtreeWidths.Init(0.0f, NodeCount);
	LayoutChildren.SetNum(NodeCount);

	// The first prerequisite found in the tree is a node's layout parent.
	TArray<int32> ParentIndices;
	ParentIndices.Init(INDEX_NONE, NodeCount);

	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		const FSkillTreeNode& Node = Tree.Nodes[Index];

		if (!Node.NodeId.IsValid())
		{
			continue;
		}

		for (const FGameplayTag& Prerequisite : Node.Prerequisites)
		{
			const int32 ParentIndex = FindNodeIndex(Tree, Prerequisite);

			if (ParentIndex != INDEX_NONE && ParentIndex != Index)
			{
				ParentIndices[Index] = ParentIndex;
				LayoutChildren[ParentIndex].Add(Index);
				break;
			}
		}
	}

	auto IsColumnRoot = [&Tree, &ParentIndices](const int32 Index)
	{
		return Tree.Nodes[Index].NodeId.IsValid() && ParentIndices[Index] == INDEX_NONE;
	};

	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		if (IsColumnRoot(Index))
		{
			MeasureSubtree(Index, 0);
		}
	}

	// Section order: as listed on the asset, then any unlisted sections in node order.
	TArray<FName> SectionOrder;

	for (const FSkillTreeSection& Section : Tree.Sections)
	{
		SectionOrder.AddUnique(Section.SectionId);
	}

	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		if (IsColumnRoot(Index))
		{
			SectionOrder.AddUnique(Tree.Nodes[Index].Section);
		}
	}

	float CursorX = ContentPadding.X;

	for (const FName SectionId : SectionOrder)
	{
		TArray<int32> Roots;

		for (int32 Index = 0; Index < NodeCount; ++Index)
		{
			if (IsColumnRoot(Index) && Tree.Nodes[Index].Section == SectionId)
			{
				Roots.Add(Index);
			}
		}

		if (Roots.IsEmpty())
		{
			continue;
		}

		FSectionLayout& SectionLayout = SectionLayouts.AddDefaulted_GetRef();
		SectionLayout.SectionId = SectionId;
		SectionLayout.DisplayName = FindSectionName(Tree, SectionId);
		SectionLayout.Position = FVector2D(CursorX, ContentPadding.Y);

		float ColumnLeft = CursorX + SectionPadding;

		for (int32 RootIndex = 0; RootIndex < Roots.Num(); ++RootIndex)
		{
			PlaceSubtree(Roots[RootIndex], ColumnLeft, 0);
			ColumnLeft += SubtreeWidths[Roots[RootIndex]];

			if (RootIndex < Roots.Num() - 1)
			{
				ColumnLeft += ColumnSpacing;
			}
		}

		SectionLayout.Size.X = ColumnLeft + SectionPadding - CursorX;
		CursorX += SectionLayout.Size.X + SectionSpacing;
	}

	const float SectionHeight = SectionHeaderHeight + SectionPadding * 2.0f + RowHeight * static_cast<float>(MaxDepth + 1);

	for (FSectionLayout& SectionLayout : SectionLayouts)
	{
		SectionLayout.Size.Y = SectionHeight;
	}

	ContentSize = FVector2D(
		FMath::Max(CursorX - SectionSpacing, ContentPadding.X) + ContentPadding.X,
		ContentPadding.Y * 2.0f + SectionHeight
	);

	// Manual overrides win over the auto-layout and can extend the content.
	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		const FSkillTreeNode& Node = Tree.Nodes[Index];

		if (!Node.NodeId.IsValid() || !Node.bUseManualPosition)
		{
			continue;
		}

		NodeCenters[Index] = Node.CanvasPosition;
		NodePlaced[Index] = true;

		const FVector2D Extent = Node.CanvasPosition + GetNodeSize(Node.Kind) * 0.5f + ContentPadding;
		ContentSize = FVector2D(FMath::Max(ContentSize.X, Extent.X), FMath::Max(ContentSize.Y, Extent.Y));
	}

	// An "OR" between neighbouring siblings of the same exclusive group.
	for (int32 ParentIndex = 0; ParentIndex < NodeCount; ++ParentIndex)
	{
		const TArray<int32>& Siblings = LayoutChildren[ParentIndex];

		for (int32 SiblingIndex = 0; SiblingIndex + 1 < Siblings.Num(); ++SiblingIndex)
		{
			const int32 Left = Siblings[SiblingIndex];
			const int32 Right = Siblings[SiblingIndex + 1];
			const FName Group = Tree.Nodes[Left].ExclusiveGroup;

			if (!Group.IsNone() &&
				Group == Tree.Nodes[Right].ExclusiveGroup &&
				NodePlaced[Left] &&
				NodePlaced[Right])
			{
				MarkerCenters.Add((NodeCenters[Left] + NodeCenters[Right]) * 0.5f);
			}
		}
	}
}

float USkillTreeWidget::MeasureSubtree(const int32 NodeIndex, const int32 Depth)
{
	float ChildrenWidth = 0.0f;

	// Depth guard: a prerequisite cycle can't recurse forever.
	if (Depth < LayoutChildren.Num())
	{
		for (const int32 ChildIndex : LayoutChildren[NodeIndex])
		{
			ChildrenWidth += MeasureSubtree(ChildIndex, Depth + 1);
		}
	}

	SubtreeWidths[NodeIndex] = FMath::Max(ColumnWidth, ChildrenWidth);
	return SubtreeWidths[NodeIndex];
}

void USkillTreeWidget::PlaceSubtree(const int32 NodeIndex, const float Left, const int32 Depth)
{
	if (Depth >= LayoutChildren.Num() || NodePlaced[NodeIndex])
	{
		return;
	}

	NodePlaced[NodeIndex] = true;
	MaxDepth = FMath::Max(MaxDepth, Depth);
	NodeCenters[NodeIndex] = FVector2D(Left + SubtreeWidths[NodeIndex] * 0.5f, GetRowCenterY(Depth));

	float ChildLeft = Left;

	for (const int32 ChildIndex : LayoutChildren[NodeIndex])
	{
		PlaceSubtree(ChildIndex, ChildLeft, Depth + 1);
		ChildLeft += SubtreeWidths[ChildIndex];
	}
}

float USkillTreeWidget::GetRowCenterY(const int32 Depth) const
{
	return ContentPadding.Y + SectionHeaderHeight + SectionPadding + RowHeight * (static_cast<float>(Depth) + 0.5f);
}

FVector2D USkillTreeWidget::GetNodeSize(const ESkillNodeKind Kind) const
{
	switch (Kind)
	{
	case ESkillNodeKind::Path:
		return PathNodeSize;

	case ESkillNodeKind::Upgrade:
		return UpgradeNodeSize;

	case ESkillNodeKind::Passive:
		return PassiveNodeSize;

	case ESkillNodeKind::Skill:
	default:
		return SkillNodeSize;
	}
}

/* -------------------- Building -------------------- */

void USkillTreeWidget::BuildSections()
{
	if (!SectionWidgetClass)
	{
		return;
	}

	for (const FSectionLayout& SectionLayout : SectionLayouts)
	{
		USkillTreeSectionWidget* SectionWidget = CreateWidget<USkillTreeSectionWidget>(this, SectionWidgetClass);

		if (!IsValid(SectionWidget))
		{
			continue;
		}

		SectionWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		SectionWidget->InitializeSection(SectionLayout.SectionId, SectionLayout.DisplayName, SectionHeaderHeight);

		if (UCanvasPanelSlot* CanvasSlot = NodeCanvas->AddChildToCanvas(SectionWidget))
		{
			CanvasSlot->SetPosition(SectionLayout.Position);
			CanvasSlot->SetSize(SectionLayout.Size);
		}

		SectionWidgets.Add(SectionWidget);
	}
}

void USkillTreeWidget::BuildLinkLayer()
{
	const TSubclassOf<USkillTreeLinkWidget> EffectiveLinkClass = LinkWidgetClass
		? LinkWidgetClass
		: TSubclassOf<USkillTreeLinkWidget>(USkillTreeLinkWidget::StaticClass());

	LinkWidget = CreateWidget<USkillTreeLinkWidget>(this, EffectiveLinkClass);

	if (!IsValid(LinkWidget))
	{
		return;
	}

	// Lines never take input; nodes underneath stay clickable.
	LinkWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	LinkWidget->UnmetColor = UnmetLinkColor;
	LinkWidget->MetColor = MetLinkColor;
	LinkWidget->LearnedColor = LearnedLinkColor;
	LinkWidget->ExcludedColor = ExcludedLinkColor;
	LinkWidget->LinkThickness = LinkThickness;

	if (UCanvasPanelSlot* CanvasSlot = NodeCanvas->AddChildToCanvas(LinkWidget))
	{
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CanvasSlot->SetOffsets(FMargin(0.0f));
	}
}

void USkillTreeWidget::BuildMarkers()
{
	if (!ExclusiveMarkerWidgetClass)
	{
		return;
	}

	for (const FVector2D& MarkerCenter : MarkerCenters)
	{
		UUserWidget* Marker = CreateWidget<UUserWidget>(this, ExclusiveMarkerWidgetClass);

		if (!IsValid(Marker))
		{
			continue;
		}

		Marker->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UCanvasPanelSlot* CanvasSlot = NodeCanvas->AddChildToCanvas(Marker))
		{
			CanvasSlot->SetSize(ExclusiveMarkerSize);
			CanvasSlot->SetPosition(MarkerCenter - ExclusiveMarkerSize * 0.5f);
		}

		MarkerWidgets.Add(Marker);
	}
}

void USkillTreeWidget::BuildNodes(const USkillTreeAsset& Tree)
{
	for (int32 Index = 0; Index < Tree.Nodes.Num(); ++Index)
	{
		const FSkillTreeNode& Node = Tree.Nodes[Index];

		if (!Node.NodeId.IsValid() || !NodePlaced[Index])
		{
			continue;
		}

		USkillTreeNodeWidget* NodeWidget = CreateWidget<USkillTreeNodeWidget>(this, NodeWidgetClass);

		if (!IsValid(NodeWidget))
		{
			continue;
		}

		NodeWidget->InitializeNode(TreeComponent, Node.NodeId);

		const FVector2D NodeSize = GetNodeSize(Node.Kind);

		if (UCanvasPanelSlot* CanvasSlot = NodeCanvas->AddChildToCanvas(NodeWidget))
		{
			CanvasSlot->SetSize(NodeSize);
			CanvasSlot->SetPosition(NodeCenters[Index] - NodeSize * 0.5f);
		}

		NodeWidgets.Add(NodeWidget);
	}
}

void USkillTreeWidget::BuildLinks(const USkillTreeAsset& Tree)
{
	Links.Reset();

	for (int32 Index = 0; Index < Tree.Nodes.Num(); ++Index)
	{
		const FSkillTreeNode& Node = Tree.Nodes[Index];

		if (!Node.NodeId.IsValid() || !NodePlaced[Index])
		{
			continue;
		}

		const float NodeHalfHeight = GetNodeSize(Node.Kind).Y * 0.5f;

		for (const FGameplayTag& Prerequisite : Node.Prerequisites)
		{
			const int32 PrerequisiteIndex = FindNodeIndex(Tree, Prerequisite);

			if (PrerequisiteIndex == INDEX_NONE || !NodePlaced[PrerequisiteIndex])
			{
				continue;
			}

			const float PrerequisiteHalfHeight = GetNodeSize(Tree.Nodes[PrerequisiteIndex].Kind).Y * 0.5f;

			FSkillTreeLink& Link = Links.AddDefaulted_GetRef();
			Link.PrerequisiteId = Prerequisite;
			Link.NodeId = Node.NodeId;
			Link.From = NodeCenters[PrerequisiteIndex] + FVector2D(0.0f, PrerequisiteHalfHeight);
			Link.To = NodeCenters[Index] - FVector2D(0.0f, NodeHalfHeight);
		}
	}
}

void USkillTreeWidget::ApplyContentSize() const
{
	if (IsValid(ContentSizeBox))
	{
		ContentSizeBox->SetWidthOverride(ContentSize.X);
		ContentSizeBox->SetHeightOverride(ContentSize.Y);
	}
}

/* -------------------- Refresh -------------------- */

void USkillTreeWidget::RefreshAllNodes()
{
	RefreshLinkStates();

	if (IsValid(LinkWidget))
	{
		LinkWidget->SetLinks(Links);
	}

	for (USkillTreeNodeWidget* NodeWidget : NodeWidgets)
	{
		if (IsValid(NodeWidget))
		{
			NodeWidget->RefreshNode();
		}
	}
}

void USkillTreeWidget::RefreshLinkStates()
{
	for (FSkillTreeLink& Link : Links)
	{
		Link.State = ResolveLinkState(Link);
	}
}

ESkillLinkState USkillTreeWidget::ResolveLinkState(const FSkillTreeLink& Link) const
{
	if (!IsValid(TreeComponent))
	{
		return ESkillLinkState::Unmet;
	}

	if (TreeComponent->GetNodeState(Link.NodeId) == ESkillNodeState::Excluded)
	{
		return ESkillLinkState::Excluded;
	}

	if (TreeComponent->GetNodeRank(Link.NodeId) > 0)
	{
		return ESkillLinkState::Learned;
	}

	return TreeComponent->IsNodeUnlocked(Link.PrerequisiteId)
		? ESkillLinkState::Met
		: ESkillLinkState::Unmet;
}