#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillTreeSectionWidget.generated.h"

/**
 * Background frame for one section of the tree. Placed and sized by
 * USkillTreeWidget behind the links and nodes; the Blueprint draws the
 * frame and puts the title inside the top HeaderHeight pixels.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API USkillTreeSectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeSection(FName InSectionId, const FText& InDisplayName, float InHeaderHeight);

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	FName GetSectionId() const { return SectionId; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	FText GetDisplayName() const { return DisplayName; }

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	float GetHeaderHeight() const { return HeaderHeight; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Tree|UI")
	void OnSectionInitialized(const FText& InDisplayName, float InHeaderHeight);

private:
	FName SectionId;
	FText DisplayName;
	float HeaderHeight = 0.0f;
};