#include "ExperienceBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "ExperienceComponent.h"
#include "GameFramework/Pawn.h"

void UExperienceBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bFindExperienceComponentAutomatically && !ExperienceComponent)
	{
		const APawn* OwningPawn = GetOwningPlayerPawn();

		if (IsValid(OwningPawn))
		{
			SetExperienceComponent(OwningPawn->FindComponentByClass<UExperienceComponent>());
		}
	}
}

void UExperienceBarWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UExperienceBarWidget::SetExperienceComponent(UExperienceComponent* InExperienceComponent)
{
	Unbind();

	ExperienceComponent = InExperienceComponent;
	ShownLevel = INDEX_NONE;
	PendingWraps = 0;

	if (!IsValid(ExperienceComponent))
	{
		return;
	}

	ExperienceComponent->OnExperienceChanged.AddDynamic(this, &UExperienceBarWidget::HandleExperienceChanged);
	ExperienceComponent->OnLevelUp.AddDynamic(this, &UExperienceBarWidget::HandleLevelUp);

	// Start from the current state without gliding up from empty.
	HandleExperienceChanged(
		ExperienceComponent->GetExperience(),
		ExperienceComponent->GetExperienceToNextLevel(),
		ExperienceComponent->GetLevel());

	SetDisplayedFill(TargetFill);
}

void UExperienceBarWidget::Unbind()
{
	if (IsValid(ExperienceComponent))
	{
		ExperienceComponent->OnExperienceChanged.RemoveDynamic(this, &UExperienceBarWidget::HandleExperienceChanged);
		ExperienceComponent->OnLevelUp.RemoveDynamic(this, &UExperienceBarWidget::HandleLevelUp);
	}

	ExperienceComponent = nullptr;
}

void UExperienceBarWidget::HandleExperienceChanged(const int32 Experience, const int32 ExperienceToNextLevel, const int32 Level)
{
	const bool bMaxLevel = ExperienceToNextLevel <= 0;

	TargetFill = bMaxLevel
		? 1.0f
		: FMath::Clamp(static_cast<float>(Experience) / static_cast<float>(ExperienceToNextLevel), 0.0f, 1.0f);

	// Every level gained since last time runs the bar to the end once.
	if (ShownLevel != INDEX_NONE && Level > ShownLevel)
	{
		PendingWraps += Level - ShownLevel;
	}

	ShownLevel = Level;

	if (!bSmoothFill)
	{
		PendingWraps = 0;
		SetDisplayedFill(TargetFill);
	}

	if (IsValid(LevelText))
	{
		LevelText->SetText(FText::FormatNamed(LevelFormat, TEXT("Level"), Level));
	}

	if (IsValid(ExperienceText))
	{
		ExperienceText->SetText(bMaxLevel
			? MaxLevelText
			: FText::FormatNamed(ExperienceFormat,
				TEXT("Experience"), Experience,
				TEXT("Required"), ExperienceToNextLevel,
				TEXT("Percent"), FText::AsPercent(TargetFill)));
	}

	OnExperienceUpdated(Experience, ExperienceToNextLevel, Level);
}

void UExperienceBarWidget::HandleLevelUp(const int32 NewLevel)
{
	OnLevelUp(NewLevel);
}

void UExperienceBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bSmoothFill)
	{
		return;
	}

	const float Step = FillSpeed * InDeltaTime;

	// Run to the end for each level gained, then start the new level from empty.
	if (PendingWraps > 0)
	{
		const float NewFill = FMath::Min(DisplayedFill + Step, 1.0f);
		SetDisplayedFill(NewFill);

		if (NewFill >= 1.0f)
		{
			--PendingWraps;
			SetDisplayedFill(0.0f);
		}

		return;
	}

	if (!FMath::IsNearlyEqual(DisplayedFill, TargetFill))
	{
		SetDisplayedFill(FMath::FInterpConstantTo(DisplayedFill, TargetFill, InDeltaTime, FillSpeed));
	}
}

void UExperienceBarWidget::SetDisplayedFill(const float Fill)
{
	DisplayedFill = Fill;

	if (IsValid(ExperienceBar))
	{
		ExperienceBar->SetPercent(DisplayedFill);
	}
}
