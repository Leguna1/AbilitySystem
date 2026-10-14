#include "SkillDetailsWidget.h"

#include "Ability.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/RichTextBlock.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace
{
	void SetShown(UWidget* Widget, const bool bShown)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	FText FormatHeader(const FText& Format, const int32 HeaderRank, const int32 HeaderMaxRank)
	{
		return FText::FormatNamed(Format, TEXT("Rank"), HeaderRank, TEXT("MaxRank"), HeaderMaxRank);
	}
}

void USkillDetailsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Until something is hovered, show the hint (or nothing).
	if (!NodeId.IsValid())
	{
		ClearDetails();
	}
}

void USkillDetailsWidget::ShowNode(USkillTreeComponent* InTreeComponent, const FGameplayTag InNodeId)
{
	TreeComponent = InTreeComponent;
	NodeId = InNodeId;
	Refresh();
}

void USkillDetailsWidget::Refresh()
{
	FSkillNodeDetails Details;

	if (!BuildDetails(Details))
	{
		ClearDetails();
		return;
	}

	SetShowingNode(true);
	ApplyToBoundWidgets(Details);
	OnDetailsUpdated(Details);
}

void USkillDetailsWidget::ClearDetails()
{
	NodeId = FGameplayTag();
	SetShowingNode(false);
	OnDetailsCleared();
}

bool USkillDetailsWidget::BuildDetails(FSkillNodeDetails& OutDetails) const
{
	const USkillTreeAsset* Tree = IsValid(TreeComponent) ? TreeComponent->GetTreeAsset() : nullptr;
	const FSkillTreeNode* Node = IsValid(Tree) && NodeId.IsValid() ? Tree->FindNode(NodeId) : nullptr;
	const UAbility* Defaults = Node && Node->AbilityClass ? Node->AbilityClass.GetDefaultObject() : nullptr;

	if (!IsValid(Defaults))
	{
		return false;
	}

	// Read from the class defaults, like the node itself: nobody needs to own the ability yet.
	OutDetails.NodeId = NodeId;
	OutDetails.DisplayName = Defaults->GetDisplayName();
	OutDetails.Icon = Defaults->GetIcon();
	OutDetails.Kind = Node->Kind;
	OutDetails.State = TreeComponent->GetNodeState(NodeId);
	OutDetails.Rank = TreeComponent->GetNodeRank(NodeId);
	OutDetails.MaxRank = TreeComponent->GetNodeMaxRank(NodeId);

	// Unlearned: next only. Learned: both. Maxed: current only.
	OutDetails.bShowCurrent = OutDetails.Rank > 0;
	OutDetails.bShowNext = OutDetails.Rank < OutDetails.MaxRank;

	if (OutDetails.bShowCurrent)
	{
		OutDetails.CurrentDescription = Defaults->GetDescriptionAtRank(OutDetails.Rank);
		OutDetails.CurrentStats = Defaults->GetDisplayStats(OutDetails.Rank);
	}

	if (OutDetails.bShowNext)
	{
		const int32 CompareToRank = bHighlightChanges ? OutDetails.Rank : 0;
		OutDetails.NextDescription = Defaults->GetDescriptionAtRank(OutDetails.Rank + 1, CompareToRank);
		OutDetails.NextStats = Defaults->GetDisplayStats(OutDetails.Rank + 1);
	}

	return true;
}

void USkillDetailsWidget::ApplyToBoundWidgets(const FSkillNodeDetails& Details)
{
	if (IsValid(NameText))
	{
		NameText->SetText(Details.DisplayName);
	}

	if (IsValid(IconImage))
	{
		IconImage->SetBrushFromTexture(Details.Icon);
		SetShown(IconImage, Details.Icon != nullptr);
	}

	SetShown(CurrentColumn, Details.bShowCurrent);

	if (Details.bShowCurrent)
	{
		if (IsValid(CurrentHeaderText))
		{
			CurrentHeaderText->SetText(FormatHeader(CurrentHeaderFormat, Details.Rank, Details.MaxRank));
		}

		if (IsValid(CurrentStatsText))
		{
			CurrentStatsText->SetText(FormatStats(Details.CurrentStats, nullptr));
			SetShown(CurrentStatsText, !Details.CurrentStats.IsEmpty());
		}

		if (IsValid(CurrentDescriptionText))
		{
			CurrentDescriptionText->SetText(Details.CurrentDescription);
		}
	}

	SetShown(NextColumn, Details.bShowNext);

	if (Details.bShowNext)
	{
		if (IsValid(NextHeaderText))
		{
			const FText& Format = Details.Rank > 0 ? NextHeaderFormat : UnlearnedHeaderFormat;
			NextHeaderText->SetText(FormatHeader(Format, Details.Rank + 1, Details.MaxRank));
		}

		if (IsValid(NextStatsText))
		{
			// Compare only against a rank the player has; the unlearned column has nothing to improve on.
			const bool bCompare = bHighlightChanges && Details.bShowCurrent;
			NextStatsText->SetText(FormatStats(Details.NextStats, bCompare ? &Details.CurrentStats : nullptr));
			SetShown(NextStatsText, !Details.NextStats.IsEmpty());
		}

		if (IsValid(NextDescriptionText))
		{
			NextDescriptionText->SetText(Details.NextDescription);
		}
	}
}

FText USkillDetailsWidget::FormatStats(const TArray<FAbilityDisplayStat>& Stats, const TArray<FAbilityDisplayStat>* CompareStats) const
{
	TArray<FText> Lines;

	for (const FAbilityDisplayStat& Stat : Stats)
	{
		FText Value = Stat.Value;

		if (CompareStats)
		{
			const FAbilityDisplayStat* Compared = CompareStats->FindByPredicate([&Stat](const FAbilityDisplayStat& Other)
			{
				return Other.Label.EqualTo(Stat.Label);
			});

			// New in this rank, or a different value: highlight it.
			if (!Compared || !Compared->Value.ToString().Equals(Value.ToString()))
			{
				Value = FText::Format(INVTEXT("<Changed>{0}</>"), Value);
			}
		}

		Lines.Add(FText::FormatNamed(StatFormat, TEXT("Label"), Stat.Label, TEXT("Value"), Value));
	}

	return FText::Join(StatSeparator, Lines);
}

void USkillDetailsWidget::SetShowingNode(const bool bShowingNode)
{
	SetShown(DetailsContent, bShowingNode);
	SetShown(EmptyHint, !bShowingNode);
}
