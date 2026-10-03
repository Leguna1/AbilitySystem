#include "SkillTreeLinkWidget.h"

void USkillTreeLinkWidget::SetLinks(const TArray<FSkillTreeLink>& InLinks)
{
	Links = InLinks;
	Invalidate(EInvalidateWidgetReason::Paint);
}

int32 USkillTreeLinkWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	// Two passes so learned paths are always drawn over dim ones where lines overlap.
	for (const FSkillTreeLink& Link : Links)
	{
		if (Link.State != ESkillLinkState::Learned)
		{
			PaintLink(Link, AllottedGeometry, OutDrawElements, LayerId);
		}
	}

	for (const FSkillTreeLink& Link : Links)
	{
		if (Link.State == ESkillLinkState::Learned)
		{
			PaintLink(Link, AllottedGeometry, OutDrawElements, LayerId);
		}
	}

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

void USkillTreeLinkWidget::PaintLink(const FSkillTreeLink& Link, const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, const int32 LayerId) const
{
	const float MidY = (Link.From.Y + Link.To.Y) * 0.5f;

	TArray<FVector2D> Points;
	Points.Reserve(4);
	Points.Add(Link.From);
	Points.Add(FVector2D(Link.From.X, MidY));
	Points.Add(FVector2D(Link.To.X, MidY));
	Points.Add(Link.To);

	FSlateDrawElement::MakeLines(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(),
		Points,
		ESlateDrawEffect::None,
		GetLinkColor(Link.State),
		true,
		LinkThickness
	);
}

FLinearColor USkillTreeLinkWidget::GetLinkColor(const ESkillLinkState State) const
{
	switch (State)
	{
	case ESkillLinkState::Met:
		return MetColor;

	case ESkillLinkState::Learned:
		return LearnedColor;

	case ESkillLinkState::Excluded:
		return ExcludedColor;

	case ESkillLinkState::Unmet:
	default:
		return UnmetColor;
	}
}