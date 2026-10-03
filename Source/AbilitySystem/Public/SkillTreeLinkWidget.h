#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillTreeWidget.h"
#include "SkillTreeLinkWidget.generated.h"

/**
 * Paints prerequisite links as elbow lines (down, across, down), colored by
 * state. Added behind the nodes and never hit-testable.
 */
UCLASS()
class ABILITYSYSTEM_API USkillTreeLinkWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLinks(const TArray<FSkillTreeLink>& InLinks);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skill Tree|Links")
	FLinearColor UnmetColor = FLinearColor(0.25f, 0.25f, 0.28f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skill Tree|Links")
	FLinearColor MetColor = FLinearColor(0.6f, 0.6f, 0.65f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skill Tree|Links")
	FLinearColor LearnedColor = FLinearColor(0.95f, 0.78f, 0.35f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skill Tree|Links")
	FLinearColor ExcludedColor = FLinearColor(0.45f, 0.12f, 0.12f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Skill Tree|Links")
	float LinkThickness = 3.0f;

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
	FLinearColor GetLinkColor(ESkillLinkState State) const;
	void PaintLink(const FSkillTreeLink& Link, const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	UPROPERTY(Transient)
	TArray<FSkillTreeLink> Links;
};