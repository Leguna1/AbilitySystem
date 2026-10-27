// UIBarMaterial.h
#pragma once

#include "CoreMinimal.h"
#include "UIBarMaterial.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * How a bar's material is driven. Lives on the bar widget (Details panel), so each
 * bar instance can use its own material instance and timings.
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEM_API FUIBarMaterialSettings
{
	GENERATED_BODY()

	/** The bar's material instance (MI_UI_Bar_Health, _Energy...). Empty = whatever the Fill image already uses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bar")
	TObjectPtr<UMaterialInterface> Material;

	/** Seconds the pale "damage taken" strip holds before it starts sliding down. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bar", meta = (ClampMin = "0.0", Units = "s"))
	float TrailDelay = 0.15f;

	/** Seconds the strip takes to slide down to the new value. 0 = no trail. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bar", meta = (ClampMin = "0.0", Units = "s"))
	float TrailDuration = 0.6f;

	/** LowPulse turns on below this fraction (health 0.25). 0 = never. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bar", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowPulseThreshold = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bar|Parameters", AdvancedDisplay)
	FName PercentParameter = TEXT("Percent");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bar|Parameters", AdvancedDisplay)
	FName TrailParameter = TEXT("Trail");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bar|Parameters", AdvancedDisplay)
	FName LowPulseParameter = TEXT("LowPulse");
};

/**
 * Runtime side of a kit bar: one dynamic material on an Image, with Percent, an eased
 * Trail after drops, and LowPulse. Plain struct, owned by the widget that ticks it.
 */
USTRUCT()
struct ABILITYSYSTEM_API FUIBarMaterial
{
	GENERATED_BODY()

	/** Makes the dynamic material on Image (applying Settings.Material first, if set). */
	void Initialize(UImage* Image, const FUIBarMaterialSettings& InSettings);

	bool IsValid() const { return Material != nullptr; }

	/** Drops leave a trail that eases down; rises and bInstant snap the trail along. */
	void SetPercent(float NewPercent, bool bInstant = false);

	/** Advances the trail. True while it is still moving (keep ticking). */
	bool Tick(float DeltaTime);

	void SetScalar(FName Parameter, float Value) const;

	float GetPercent() const { return Percent; }

	UMaterialInstanceDynamic* GetMaterial() const { return Material; }

private:
	void Apply() const;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	FUIBarMaterialSettings Settings;

	float Percent = 1.0f;
	float Trail = 1.0f;
	float TrailFrom = 1.0f;

	/** Time since the last drop; the trail moves once it passes TrailDelay. */
	float TrailElapsed = 0.0f;
	bool bTrailMoving = false;
};
