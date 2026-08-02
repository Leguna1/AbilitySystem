#include "SkillTreeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "SkillTreeAsset.h"
#include "SkillTreeComponent.h"
#include "SkillTreeLinkWidget.h"
#include "SkillTreeNodeWidget.h"

void USkillTreeWidget::InitializeTree(USkillTreeComponent* InTreeComponent)
{
	// Detach from any previous component.
	if (IsValid(TreeComponent))
	{
		TreeComponent->OnSkillTreeChanged.RemoveDynamic(this, &USkillTreeWidget::HandleTreeChanged);
	}

	TreeComponent = InTreeComponent;

	if (IsValid(TreeComponent))
	{
		// One subscription drives all refreshes: any unlock/refund/point change
		// re-evaluates the whole tree, because one node changing can flip its
		// neighbours between Locked and Available.
		TreeComponent->OnSkillTreeChanged.AddDynamic(this, &USkillTreeWidget::HandleTreeChanged);
	}

	RebuildTree();
}

void USkillTreeWidget::RebuildTree()
{
	ClearNodes();

	if (!IsValid(TreeComponent) || !IsValid(NodeCanvas) || !IsValid(NodeWidgetClass))
	{
		return;
	}

	BuildNodes();
	BuildLinks();
	RefreshAllNodes();
}

void USkillTreeWidget::BuildNodes()
{
	const USkillTreeAsset* Tree = TreeComponent->GetTreeAsset();
	if (!IsValid(Tree))
	{
		return;
	}

	// PROBLEM 2 (ordering) - LINK WIDGET FIRST.
	// Add the line-painting widget as the FIRST canvas child so every node added
	// after it renders on top. A canvas paints children in add order, so this
	// guarantees nodes-over-lines without relying on NativePaint layer ids.
	TSubclassOf<USkillTreeLinkWidget> EffectiveLinkClass = LinkWidgetClass;
	if (!EffectiveLinkClass)
	{
		EffectiveLinkClass = USkillTreeLinkWidget::StaticClass();
	}

	LinkWidget = CreateWidget<USkillTreeLinkWidget>(this, EffectiveLinkClass);
	if (IsValid(LinkWidget))
	{
		LinkWidget->MetLinkColor = MetLinkColor;
		LinkWidget->UnmetLinkColor = UnmetLinkColor;
		LinkWidget->LinkThickness = LinkThickness;

		if (UCanvasPanelSlot* LinkSlot = NodeCanvas->AddChildToCanvas(LinkWidget))
		{
			// Stretch to fill the canvas so link coordinates share the node space.
			LinkSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			LinkSlot->SetOffsets(FMargin(0.0f));
		}
	}

	// PROBLEM 1 - PLACEMENT.
	// For each node in the data, spawn a node widget and position it on the
	// canvas at its authored CanvasPosition (scaled to pixels). The Canvas Panel
	// is the only UMG container that positions children by explicit coordinates.
	for (const FSkillTreeNode& Node : Tree->Nodes)
	{
		if (!Node.NodeId.IsValid())
		{
			continue;
		}

		USkillTreeNodeWidget* NodeWidget = CreateWidget<USkillTreeNodeWidget>(this, NodeWidgetClass);
		if (!IsValid(NodeWidget))
		{
			continue;
		}

		NodeWidget->InitializeNode(TreeComponent, Node.NodeId);

		if (UCanvasPanelSlot* CanvasSlot = NodeCanvas->AddChildToCanvas(NodeWidget))
		{
			CanvasSlot->SetSize(NodeSize);
			// Top-left corner = authored position * scale. Node center falls at
			// position*scale + NodeSize/2, which NodeCenter() mirrors for links.
			CanvasSlot->SetPosition(Node.CanvasPosition * PositionScale);
		}

		NodeWidgets.Add(NodeWidget);
	}
}

void USkillTreeWidget::BuildLinks()
{
	Links.Reset();

	const USkillTreeAsset* Tree = TreeComponent->GetTreeAsset();
	if (!IsValid(Tree))
	{
		return;
	}

	// PROBLEM 2 - CONNECTIONS.
	// A link is pure geometry between two positions we already know. For every
	// node, for every prerequisite it lists, make a line from this node's center
	// to that prerequisite's center. No hand-drawing: links fall out of the data.
	for (const FSkillTreeNode& Node : Tree->Nodes)
	{
		const FVector2D NodeC = NodeCenter(Node.CanvasPosition);

		for (const FGameplayTag& PrereqId : Node.Prerequisites)
		{
			const FSkillTreeNode* Prereq = Tree->FindNode(PrereqId);
			if (!Prereq)
			{
				continue;
			}

			FSkillTreeLink Link;
			Link.From = NodeC;
			Link.To = NodeCenter(Prereq->CanvasPosition);
			Link.bPrerequisiteMet = TreeComponent->IsNodeUnlocked(PrereqId);
			Links.Add(Link);
		}
	}
}

FVector2D USkillTreeWidget::NodeCenter(const FVector2D& AuthoredPosition) const
{
	return AuthoredPosition * PositionScale + NodeSize * 0.5f;
}

void USkillTreeWidget::RefreshAllNodes()
{
	// Rebuild link "met" flags too, so satisfied prerequisites recolor.
	BuildLinks();

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

void USkillTreeWidget::HandleTreeChanged()
{
	RefreshAllNodes();
}

void USkillTreeWidget::ClearNodes()
{
	for (USkillTreeNodeWidget* NodeWidget : NodeWidgets)
	{
		if (IsValid(NodeWidget))
		{
			NodeWidget->RemoveFromParent();
		}
	}

	NodeWidgets.Reset();
	Links.Reset();
	LinkWidget = nullptr;

	if (IsValid(NodeCanvas))
	{
		NodeCanvas->ClearChildren();
	}
}

void USkillTreeWidget::NativeDestruct()
{
	if (IsValid(TreeComponent))
	{
		TreeComponent->OnSkillTreeChanged.RemoveDynamic(this, &USkillTreeWidget::HandleTreeChanged);
	}

	Super::NativeDestruct();
}