#include "ResourceWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UResourceWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (bFindResourceComponentAutomatically)
	{
		FindOwningResourceComponent();
	}

	BindToResourceComponent();
	RefreshResourceBar();
}

void UResourceWidget::NativeDestruct()
{
	UnbindFromResourceComponent();

	Super::NativeDestruct();
}

void UResourceWidget::SetResourceComponent(
	UResourceComponent* NewResourceComponent)
{
	if (ResourceComponent == NewResourceComponent)
	{
		RefreshResourceBar();
		return;
	}

	UnbindFromResourceComponent();

	ResourceComponent = NewResourceComponent;

	BindToResourceComponent();
	RefreshResourceBar();
}

void UResourceWidget::SetDisplayedResource(
	const EResourceType NewResourceType)
{
	if (DisplayedResource == NewResourceType)
	{
		return;
	}

	DisplayedResource = NewResourceType;

	RefreshResourceBar();
}

void UResourceWidget::FindOwningResourceComponent()
{
	APawn* OwningPawn = GetOwningPlayerPawn();

	if (!OwningPawn)
	{
		APlayerController* OwningController = GetOwningPlayer();

		if (OwningController)
		{
			OwningPawn = OwningController->GetPawn();
		}
	}

	if (!OwningPawn)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("%s could not find its owning pawn."),
			*GetName()
		);

		return;
	}

	ResourceComponent =
		OwningPawn->FindComponentByClass<UResourceComponent>();

	if (!ResourceComponent)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT(
				"%s could not find UMyResourceComponent on %s."
			),
			*GetName(),
			*OwningPawn->GetName()
		);
	}
}

void UResourceWidget::BindToResourceComponent()
{
	if (!ResourceComponent)
	{
		return;
	}

	ResourceComponent->OnResourceChanged.RemoveDynamic(
		this,
		&UResourceWidget::HandleResourceChanged
	);

	ResourceComponent->OnResourceChanged.AddDynamic(
		this,
		&UResourceWidget::HandleResourceChanged
	);
}

void UResourceWidget::UnbindFromResourceComponent()
{
	if (!ResourceComponent)
	{
		return;
	}

	ResourceComponent->OnResourceChanged.RemoveDynamic(
		this,
		&UResourceWidget::HandleResourceChanged
	);
}

void UResourceWidget::HandleResourceChanged(
	const EResourceType ResourceType,
	const EResourceValueType ValueType,
	const float OldValue,
	const float NewValue)
{
	if (ResourceType != DisplayedResource)
	{
		return;
	}

	// Both Current and Max changes affect the displayed percentage.
	RefreshResourceBar();
}

void UResourceWidget::RefreshResourceBar()
{
	if (!ResourceProgressBar)
	{
		return;
	}

	if (!ResourceComponent)
	{
		ResourceProgressBar->SetPercent(0.0f);

		if (ResourceValueText)
		{
			ResourceValueText->SetText(FText::GetEmpty());
		}

		return;
	}

	const float CurrentValue =
		ResourceComponent->GetResourceValue(
			DisplayedResource,
			EResourceValueType::Current
		);

	const float MaxValue =
		ResourceComponent->GetResourceValue(
			DisplayedResource,
			EResourceValueType::Max
		);

	const float Percent =
		MaxValue > 0.0f
			? FMath::Clamp(CurrentValue / MaxValue, 0.0f, 1.0f)
			: 0.0f;

	ResourceProgressBar->SetPercent(Percent);

	if (ResourceValueText)
	{
		ResourceValueText->SetVisibility(bShowValueText? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		if (bShowValueText)
		{
			ResourceValueText->SetText(MakeResourceValueText(CurrentValue, MaxValue));
		}
	}

	OnResourceBarRefreshed(CurrentValue, MaxValue, Percent);
}

FText UResourceWidget::MakeResourceValueText(const float CurrentValue,const float MaxValue) const
{
	FNumberFormattingOptions FormattingOptions;

	FormattingOptions.MinimumFractionalDigits = DecimalPlaces;
	FormattingOptions.MaximumFractionalDigits = DecimalPlaces;

	const FText CurrentText = FText::AsNumber(CurrentValue, &FormattingOptions);

	const FText MaxText = FText::AsNumber(MaxValue, &FormattingOptions);

	return FText::Format(NSLOCTEXT("ResourceBar", "CurrentAndMax", "{0} / {1}"),	CurrentText,	MaxText);
}