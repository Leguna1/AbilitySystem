#include "CharacterPreviewWidget.h"

#include "AbilitySystemLog.h"
#include "CharacterPreviewStudio.h"
#include "Components/Image.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"

void UCharacterPreviewWidget::SetStudio(ACharacterPreviewStudio* InStudio)
{
	Studio = InStudio;
	ApplyRenderTarget();
}

void UCharacterPreviewWidget::ApplyRenderTarget()
{
	if (!IsValid(PreviewImage))
	{
		return;
	}

	UTextureRenderTarget2D* RenderTarget = Studio.IsValid() ? Studio->GetRenderTarget() : nullptr;

	if (UMaterialInstanceDynamic* Material = PreviewImage->GetDynamicMaterial())
	{
		Material->SetTextureParameterValue(TextureParameterName, RenderTarget);
		return;
	}

	// No material on the brush: show the texture directly. Works, but the background isn't transparent.
	if (IsValid(RenderTarget))
	{
		if (!bWarnedNoMaterial)
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("%s: PreviewImage has no material; the preview's background won't be transparent."), *GetName());
			bWarnedNoMaterial = true;
		}

		PreviewImage->SetBrushResourceObject(RenderTarget);
	}
}

FReply UCharacterPreviewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !Studio.IsValid())
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	bDragging = true;
	LastDragPosition = InMouseEvent.GetScreenSpacePosition();

	// Captured, so the drag keeps going when the cursor leaves the image.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UCharacterPreviewWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	const FVector2D Position = InMouseEvent.GetScreenSpacePosition();
	const float DeltaX = static_cast<float>(Position.X - LastDragPosition.X);
	LastDragPosition = Position;

	if (ACharacterPreviewStudio* ShownStudio = Studio.Get())
	{
		// Dragging right turns the subject's front to the right, like spinning a figure on a turntable.
		ShownStudio->AddSubjectYaw((bInvertDrag ? DeltaX : -DeltaX) * RotationSpeed);
	}

	return FReply::Handled();
}

FReply UCharacterPreviewWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bDragging || InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

void UCharacterPreviewWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bDragging = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}
