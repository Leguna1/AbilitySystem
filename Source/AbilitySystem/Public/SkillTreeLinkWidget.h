#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillTreeWidget.h"
#include "SkillTreeLinkWidget.generated.h"

/**
 * Paints the prerequisite links. Added as the FIRST child of the tree canvas so
 * that, because a canvas paints children in add order, every node added after it
 * renders on top of the lines. This avoids NativePaint layer-ordering pitfalls.
 */
UCLASS()
class ABILITYSYSTEM_API USkillTreeLinkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Replaces the links to draw and requests a repaint. */
	void SetLinks(const TArray<FSkillTreeLink>& InLinks);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skill Tree|Links")
	FLinearColor MetLinkColor = FLinearColor(0.9f, 0.75f, 0.35f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skill Tree|Links")
	FLinearColor UnmetLinkColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skill Tree|Links")
	float LinkThickness = 2.0f;

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	UPROPERTY(Transient)
	TArray<FSkillTreeLink> Links;
};