#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillPointsWidget.generated.h"

class USkillTreeComponent;
class UTextBlock;

/** Shows the available skill points and updates whenever they change. */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API USkillPointsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Skill Tree|UI")
	void InitializeSkillPoints(USkillTreeComponent* InTreeComponent);

	UFUNCTION(BlueprintPure, Category = "Skill Tree|UI")
	int32 GetAvailablePoints() const { return AvailablePoints; }

protected:
	virtual void NativeDestruct() override;

	/** Optional text block named "PointsText", filled from PointsFormat. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Tree|UI")
	TObjectPtr<UTextBlock> PointsText;

	/** {0} is replaced with the point count. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree|UI")
	FText PointsFormat = NSLOCTEXT("SkillPoints", "Format", "Skill Points: {0}");

	/** Called on init and on every change, for extra visuals (e.g. a glow when points are available). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Tree|UI")
	void OnPointsChanged(int32 NewPoints);

private:
	UFUNCTION()
	void HandleSkillPointsChanged(int32 NewAvailablePoints);

	void SetPoints(int32 NewPoints);

	UPROPERTY(Transient)
	TObjectPtr<USkillTreeComponent> TreeComponent;

	int32 AvailablePoints = 0;
};