#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterPreviewWidget.generated.h"

class ACharacterPreviewStudio;
class UImage;

/**
 * Shows the character menu's 3D preview: the preview studio's render target in an Image.
 * Drag with the left mouse button to turn the subject.
 *
 * The Character Menu hands every preview widget inside it the studio when it opens; which subject is
 * shown (player or companion) is set per tab in the menu's Tabs list.
 *
 * Widget, optional: PreviewImage. Give it a UI material with a Texture Parameter named Texture Parameter Name
 * (PreviewTexture): Final Color = RGB, Opacity = 1 - A (Scene Color HDR stores coverage inverted).
 * Its Visibility must be Visible so it receives the drag.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UCharacterPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows Studio's render target and turns its subject on drag. Null shows nothing. */
	UFUNCTION(BlueprintCallable, Category = "Character Preview")
	void SetStudio(ACharacterPreviewStudio* InStudio);

	UFUNCTION(BlueprintPure, Category = "Character Preview")
	ACharacterPreviewStudio* GetStudio() const { return Studio.Get(); }

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Character Preview")
	TObjectPtr<UImage> PreviewImage;

	/** The Texture Parameter in PreviewImage's material that receives the render target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Preview")
	FName TextureParameterName = TEXT("PreviewTexture");

	/** Degrees the subject turns per pixel dragged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Preview", meta = (ClampMin = "0.0"))
	float RotationSpeed = 0.5f;

	/** Turn the other way when dragging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Preview")
	bool bInvertDrag = false;

private:
	void ApplyRenderTarget();

	TWeakObjectPtr<ACharacterPreviewStudio> Studio;

	FVector2D LastDragPosition = FVector2D::ZeroVector;
	bool bDragging = false;
	bool bWarnedNoMaterial = false;
};
