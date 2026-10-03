#include "SkillTreeSectionWidget.h"

void USkillTreeSectionWidget::InitializeSection(const FName InSectionId, const FText& InDisplayName, const float InHeaderHeight)
{
	SectionId = InSectionId;
	DisplayName = InDisplayName;
	HeaderHeight = InHeaderHeight;

	OnSectionInitialized(DisplayName, HeaderHeight);
}