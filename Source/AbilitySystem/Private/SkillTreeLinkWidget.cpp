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
	for (const FSkillTreeLink& Link : Links)
	{
		const FLinearColor LineColor = Link.bPrerequisiteMet ? MetLinkColor : UnmetLinkColor;

		TArray<FVector2D> Points;
		Points.Add(Link.From);
		Points.Add(Link.To);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			Points,
			ESlateDrawEffect::None,
			LineColor,
			true,
			LinkThickness
		);
	}

	return Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect,
		OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}