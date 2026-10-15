#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExperienceBarWidget.generated.h"

class UExperienceComponent;
class UProgressBar;
class UTextBlock;

/**
 * XP bar for an Experience Component: fill, level and numbers.
 * Finds the owning player's component by itself, or call Set Experience Component.
 *
 * Optional widgets in the UMG subclass, found by name:
 *  - ExperienceBar (Progress Bar), LevelText (Text), ExperienceText (Text)
 * With Smooth Fill on, the bar glides to new values; a level up fills it to the end, wraps and continues.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UExperienceBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Experience")
	void SetExperienceComponent(UExperienceComponent* InExperienceComponent);

	UFUNCTION(BlueprintPure, Category = "Experience")
	UExperienceComponent* GetExperienceComponent() const { return ExperienceComponent; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Values changed. Bound widgets are already updated; use this for anything extra. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Experience")
	void OnExperienceUpdated(int32 Experience, int32 ExperienceToNextLevel, int32 Level);

	/** A level was gained: play a flash or an animation here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Experience")
	void OnLevelUp(int32 NewLevel);

	/** Look for the Experience Component on the owning player's pawn at construct. Off = call Set Experience Component. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience")
	bool bFindExperienceComponentAutomatically = true;

	/** {Level} is filled in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience|Format")
	FText LevelFormat = NSLOCTEXT("ExperienceBar", "LevelFormat", "Lv {Level}");

	/** {Experience}, {Required} and {Percent} are filled in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience|Format")
	FText ExperienceFormat = NSLOCTEXT("ExperienceBar", "ExperienceFormat", "{Experience} / {Required}");

	/** Shown instead of the numbers at max level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience|Format")
	FText MaxLevelText = NSLOCTEXT("ExperienceBar", "MaxLevel", "MAX");

	/** Glide the fill instead of jumping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience|Fill")
	bool bSmoothFill = true;

	/** Fill moved per second while gliding (1 = a full bar per second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Experience|Fill", meta = (ClampMin = "0.1", EditCondition = "bSmoothFill"))
	float FillSpeed = 1.5f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Experience")
	TObjectPtr<UProgressBar> ExperienceBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Experience")
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Experience")
	TObjectPtr<UTextBlock> ExperienceText;

private:
	UFUNCTION()
	void HandleExperienceChanged(int32 Experience, int32 ExperienceToNextLevel, int32 Level);

	UFUNCTION()
	void HandleLevelUp(int32 NewLevel);

	void Unbind();
	void SetDisplayedFill(float Fill);

	UPROPERTY(Transient)
	TObjectPtr<UExperienceComponent> ExperienceComponent;

	float DisplayedFill = 0.0f;
	float TargetFill = 0.0f;

	/** Level ups the fill still has to run through to the end of the bar. */
	int32 PendingWraps = 0;

	/** Level the bar last showed, so a gain of several levels wraps that many times. */
	int32 ShownLevel = INDEX_NONE;
};
