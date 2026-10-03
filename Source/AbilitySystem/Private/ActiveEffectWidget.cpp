#include "ActiveEffectWidget.h"

void UActiveEffectWidget::InitializeEffect(const FActiveEffectInfo& InInfo)
{
	Info = InInfo;
	SourceKey = FObjectKey(InInfo.Source.Get());

	ToolTipWidgetDelegate.BindDynamic(this, &UActiveEffectWidget::GetTooltipWidget);

	OnEffectInitialized(Info);
	OnEffectUpdated(Info.RemainingTime, Info.Duration, Info.Stacks);
}

void UActiveEffectWidget::UpdateEffect(const FActiveEffectInfo& InInfo)
{
	// More time left than a moment ago, or more stacks: the effect was triggered again.
	const bool bRefreshed =
		(InInfo.RemainingTime >= 0.0f && InInfo.RemainingTime > Info.RemainingTime + 0.05f) ||
		InInfo.Stacks > Info.Stacks;

	Info = InInfo;
	OnEffectUpdated(Info.RemainingTime, Info.Duration, Info.Stacks);

	if (bRefreshed)
	{
		OnEffectRefreshed();
	}
}

bool UActiveEffectWidget::Matches(const FActiveEffectInfo& Other) const
{
	return SourceKey == FObjectKey(Other.Source.Get()) && Info.EntryKey == Other.EntryKey;
}

UWidget* UActiveEffectWidget::GetTooltipWidget()
{
	return BuildTooltipWidget();
}