#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModifierTypes.h"
#include "ActiveEffectWidget.generated.h"

/** One active effect on the effects bar. C++ feeds the data; the Blueprint draws it. */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UActiveEffectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeEffect(const FActiveEffectInfo& InInfo);
	void UpdateEffect(const FActiveEffectInfo& InInfo);
	bool Matches(const FActiveEffectInfo& Other) const;

	UFUNCTION(BlueprintPure, Category = "Effect|UI")
	const FActiveEffectInfo& GetEffectInfo() const { return Info; }

protected:
	/** Populate icon and name here. Called once. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Effect|UI")
	void OnEffectInitialized(const FActiveEffectInfo& EffectInfo);

	/** Called on every update. RemainingTime below 0 = untimed (hide the countdown). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Effect|UI")
	void OnEffectUpdated(float RemainingTime, float Duration, int32 Stacks);

	/** The effect was re-triggered (duration restarted or stacks increased). Good for a flash. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Effect|UI")
	void OnEffectRefreshed();

	/** Return a tooltip widget for hover (read GetEffectInfo). Null = no tooltip. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Effect|UI")
	UWidget* BuildTooltipWidget();

private:
	UFUNCTION()
	UWidget* GetTooltipWidget();

	UPROPERTY(Transient)
	FActiveEffectInfo Info;

	FObjectKey SourceKey;
};