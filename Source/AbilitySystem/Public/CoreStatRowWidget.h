#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "CoreStatRowWidget.generated.h"

class UAttributePanelWidget;
class UButton;
class UTextBlock;

/**
 * One core stat on an Attribute Panel: "Strength  14  [+]".
 * The + button spends a point; hovering the row previews what a point adds to every attribute row.
 * Optional widgets: LabelText, ValueText, AddButton.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UCoreStatRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UAttributePanelWidget* InPanel, FGameplayTag InCoreStat, const FText& Label, const FText& Description);

	/** Current value, and whether a point can go in now. */
	void Refresh(float Value, bool bCanSpend, bool bSpendingAllowed);

	UFUNCTION(BlueprintPure, Category = "Core Stat Row")
	FGameplayTag GetCoreStat() const { return CoreStat; }

	UFUNCTION(BlueprintPure, Category = "Core Stat Row")
	FText GetDescription() const { return Description; }

	/** Spends one point here. Also callable from a Blueprint button of your own. */
	UFUNCTION(BlueprintCallable, Category = "Core Stat Row")
	void SpendPoint();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Core Stat Row")
	TObjectPtr<UTextBlock> LabelText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Core Stat Row")
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Core Stat Row")
	TObjectPtr<UButton> AddButton;

	/** Read-only panels (the companion's) hide the button; others enable it while a point can be spent. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Core Stat Row")
	void OnSpendStateChanged(bool bCanSpend, bool bSpendingAllowed);

private:
	UFUNCTION()
	void HandleAddClicked();

	TWeakObjectPtr<UAttributePanelWidget> Panel;
	FGameplayTag CoreStat;
	FText Description;
};
