#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CompanionMenuWidget.generated.h"

class ACompanionCharacter;
class APawn;
class UAbilityBarWidget;
class UAttributePanelWidget;
class UCompanionComponent;
class UImage;
class UPoiseBarWidget;
class UResourceWidget;
class UTextBlock;
class UWidget;

/**
 * The Companion tab of the character menu: who your companion is, its health and poise, its abilities,
 * and (with a Character Preview Widget inside) the 3D preview with the companion as subject.
 * With no companion out, Content Panel is hidden and Empty State Panel shown instead.
 *
 * Widgets, all optional:
 *   ContentPanel, EmptyStatePanel: shown with and without a companion.
 *   NameText, LevelText, Portrait.
 *   HealthBar (Resource Widget: Displayed Resource Health, Find Resource Component Automatically OFF).
 *   PoiseBar (Poise Bar Widget: Find Combatant Automatically OFF).
 *   AbilityBar (Ability Bar Widget, the companion slot class).
 *   CompanionStats (Attribute Panel, Allow Spending off).
 * Place a Character Preview Widget anywhere inside; the menu hands it the studio, and the tab's
 * Preview Subject (Companion) puts the companion in it.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UCompanionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Follows Leader's companion (the player's pawn). Called by the menu; also on construct for the owning pawn. */
	UFUNCTION(BlueprintCallable, Category = "Companion Menu")
	void SetLeader(APawn* Leader);

	/** Re-reads name, level and portrait (the menu calls this each time it opens). */
	UFUNCTION(BlueprintCallable, Category = "Companion Menu")
	void RefreshCompanion();

	UFUNCTION(BlueprintPure, Category = "Companion Menu")
	ACompanionCharacter* GetShownCompanion() const { return ShownCompanion.Get(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Everything shown while a companion is out. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UWidget> ContentPanel;

	/** Shown while no companion is out ("Summon your companion to see it here"). */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UWidget> EmptyStatePanel;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UTextBlock> LevelText;

	/** Gets the companion's Character Portrait (left as designed if it has none). */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UImage> Portrait;

	/** Set its Find Resource Component Automatically off, or it shows the player's health. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UResourceWidget> HealthBar;

	/** Set its Find Combatant Automatically off, or it shows the player's poise. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UPoiseBarWidget> PoiseBar;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UAbilityBarWidget> AbilityBar;

	/** The companion's stats. Turn Allow Spending off on it: companions don't spend points. */
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Companion Menu")
	TObjectPtr<UAttributePanelWidget> CompanionStats;

	/** Format of LevelText. {Level} is replaced. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Companion Menu")
	FText LevelFormat = NSLOCTEXT("CompanionMenu", "LevelFormat", "Level {Level}");

	/** A companion came out (Companion set) or went away (null). Panels are already switched. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Companion Menu")
	void OnCompanionShown(ACompanionCharacter* Companion);

private:
	UFUNCTION()
	void HandleCompanionChanged(ACompanionCharacter* Companion);

	/** Points every part at Companion (null = empty state). */
	void ShowCompanion(ACompanionCharacter* Companion);

	void UnbindLeader();

	TWeakObjectPtr<UCompanionComponent> BoundCompanions;
	TWeakObjectPtr<ACompanionCharacter> ShownCompanion;
};
