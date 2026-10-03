#include "SkillPointsWidget.h"

#include "Components/TextBlock.h"
#include "SkillTreeComponent.h"

void USkillPointsWidget::InitializeSkillPoints(USkillTreeComponent* InTreeComponent)
{
	if (IsValid(TreeComponent))
	{
		TreeComponent->OnSkillPointsChanged.RemoveDynamic(this, &USkillPointsWidget::HandleSkillPointsChanged);
	}

	TreeComponent = InTreeComponent;

	if (IsValid(TreeComponent))
	{
		TreeComponent->OnSkillPointsChanged.AddDynamic(this, &USkillPointsWidget::HandleSkillPointsChanged);
	}

	SetPoints(IsValid(TreeComponent) ? TreeComponent->GetAvailableSkillPoints() : 0);
}

void USkillPointsWidget::NativeDestruct()
{
	if (IsValid(TreeComponent))
	{
		TreeComponent->OnSkillPointsChanged.RemoveDynamic(this, &USkillPointsWidget::HandleSkillPointsChanged);
	}

	Super::NativeDestruct();
}

void USkillPointsWidget::HandleSkillPointsChanged(const int32 NewAvailablePoints)
{
	SetPoints(NewAvailablePoints);
}

void USkillPointsWidget::SetPoints(const int32 NewPoints)
{
	AvailablePoints = NewPoints;

	if (IsValid(PointsText))
	{
		PointsText->SetText(FText::Format(PointsFormat, FText::AsNumber(AvailablePoints)));
	}

	OnPointsChanged(AvailablePoints);
}