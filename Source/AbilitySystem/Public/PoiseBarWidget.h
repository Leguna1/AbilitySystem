#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PoiseBarWidget.generated.h"

class UCombatantComponent;
class UProgressBar;

/**
 * Shows a combatant's poise. Goes under an enemy's health bar (the nameplate hands it the enemy)
 * or on the HUD (finds the player's pawn by itself).
 *
 * Poise recovers without events, so the bar reads it every frame while it's below full,
 * and sleeps (optionally hidden) once full.
 * A break shows as an empty bar for Broken Hold Time, then it fills back up.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UPoiseBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Poise Bar")
	void SetCombatantComponent(UCombatantComponent* NewCombatantComponent);

	UFUNCTION(BlueprintPure, Category = "Poise Bar")
	UCombatantComponent* GetCombatantComponent() const { return CombatantComponent; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Must be named PoiseBar. */
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Poise Bar|Widgets")
	TObjectPtr<UProgressBar> PoiseBar;

	/** HUD: find the owning player's combatant. Off for nameplates (the enemy hands itself over). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise Bar")
	bool bFindCombatantAutomatically = true;

	/** Collapsed while poise is full: it only appears once something is chipping at it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise Bar")
	bool bHideWhenFull = true;

	/** Seconds a full bar stays visible before hiding. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise Bar", meta = (ClampMin = "0.0", EditCondition = "bHideWhenFull", Units = "s"))
	float HideDelay = 1.0f;

	/** After a break the bar stays empty this long, so the break reads before the refill. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise Bar", meta = (ClampMin = "0.0", Units = "s"))
	float BrokenHoldTime = 0.6f;

	/** How fast the bar rises toward the real value (drops are instant). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise Bar", meta = (ClampMin = "0.1"))
	float FillInterpSpeed = 6.0f;

	/** A hit took poise. Flash the bar, shake it... */
	UFUNCTION(BlueprintImplementableEvent, Category = "Poise Bar")
	void OnPoiseDamaged(float Percent);

	/** Poise broke: the target staggers. A good moment for a bright flash or a "BREAK" text. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Poise Bar")
	void OnPoiseBroken();

private:
	UFUNCTION()
	void HandlePoiseDamaged(float Poise, float MaxPoise, bool bBroken);

	void Bind();
	void Unbind();
	void SetBarVisible(bool bVisible);

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> CombatantComponent;

	float DisplayedPercent = 1.0f;
	float BrokenHoldRemaining = 0.0f;
	float FullTime = 0.0f;
};
