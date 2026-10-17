#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "PayloadReceiver.h"
#include "AttributePanelWidget.generated.h"

class UAttributeComponent;
class UAttributeRowWidget;
class UCombatantComponent;
class UCoreStatRowWidget;
class UPanelWidget;
class UTextBlock;

/** How an attribute's number is written. */
UENUM(BlueprintType)
enum class EAttributeDisplayFormat : uint8
{
	/** 42 */
	Number,

	/** The value as a percentage: 1.25 -> 125% (attack speed). */
	Percent,

	/** Signed percentage of a 1-based multiplier: 1.25 -> +25%, 0.9 -> -10%. */
	BonusPercent
};

/** One row of an Attribute Panel. */
USTRUCT(BlueprintType)
struct FAttributeDisplayEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	FText Label;

	/** The attribute shown. For a range, its low end (Stat.AttackPower.Min). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute", meta = (Categories = "Stat"))
	FGameplayTag Attribute;

	/** Optional high end: shows "Min – Max" (Stat.AttackPower.Max). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute", meta = (Categories = "Stat"))
	FGameplayTag RangeMaxAttribute;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	EAttributeDisplayFormat Format = EAttributeDisplayFormat::Number;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute", meta = (ClampMin = "0", ClampMax = "3"))
	int32 Decimals = 0;

	/** Adds the damage it blocks: "120 (37%)". For Armor (Physical) and Magic Resist (Magic). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	bool bShowDamageReduction = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute", meta = (EditCondition = "bShowDamageReduction"))
	EDamageType ReductionType = EDamageType::Physical;

	/** Hidden on characters that don't have the attribute (a wolf without Magic Power). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute")
	bool bHideIfMissing = true;
};

/**
 * A character's stats, for the character sheet. Works for anyone with an Attribute Component:
 * put one in the sheet for yourself and one for your companion.
 *
 *   - Rows: the Entries list, in order, as Attribute Row widgets in RowContainer. Live on every change.
 *   - Core stats: one Core Stat Row per core stat in CoreStatContainer (optional), with + buttons
 *     when Allow Spending is on. Hovering one previews what a point adds to every row.
 *   - No character (companion not out): rows cleared and On Availability Changed(false), to grey it out.
 *
 * Widgets, all optional except RowContainer: CoreStatContainer, LevelText, UnspentPointsText.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UAttributePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows Character's stats. Null clears the panel (and greys it out through On Availability Changed). */
	UFUNCTION(BlueprintCallable, Category = "Attribute Panel")
	void SetCharacter(AActor* Character);

	UFUNCTION(BlueprintPure, Category = "Attribute Panel")
	AActor* GetCharacter() const { return ShownCharacter.Get(); }

	UFUNCTION(BlueprintPure, Category = "Attribute Panel")
	bool HasCharacter() const { return ShownCharacter.IsValid(); }

	/** Rows show what one point in CoreStat would add. Empty tag clears the preview. */
	UFUNCTION(BlueprintCallable, Category = "Attribute Panel")
	void SetPreviewCoreStat(FGameplayTag CoreStat);

	UFUNCTION(BlueprintCallable, Category = "Attribute Panel")
	bool SpendPoint(FGameplayTag CoreStat);

	/** Re-reads everything. Normally automatic. */
	UFUNCTION(BlueprintCallable, Category = "Attribute Panel")
	void RefreshAll();

protected:
	virtual void NativeDestruct() override;

	/** The rows, in order. Leave out what this panel shouldn't show. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Panel", meta = (TitleProperty = "Label"))
	TArray<FAttributeDisplayEntry> Entries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute Panel")
	TSubclassOf<UAttributeRowWidget> RowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute Panel")
	TSubclassOf<UCoreStatRowWidget> CoreStatRowClass;

	/** + buttons on core stats. Off for read-only panels (the companion's). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Panel")
	bool bAllowSpending = true;

	/** Format of the Level text. {Level} is replaced. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Panel")
	FText LevelFormat = NSLOCTEXT("AttributePanel", "LevelFormat", "Level {Level}");

	/** Format of the unspent points text. {Points} is replaced. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute Panel")
	FText UnspentPointsFormat = NSLOCTEXT("AttributePanel", "PointsFormat", "Points: {Points}");

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly, Category = "Attribute Panel")
	TObjectPtr<UPanelWidget> RowContainer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Attribute Panel")
	TObjectPtr<UPanelWidget> CoreStatContainer;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Attribute Panel")
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly, Category = "Attribute Panel")
	TObjectPtr<UTextBlock> UnspentPointsText;

	/** A character was set (true) or cleared (false): grey out, show "No companion", disable a tab... */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attribute Panel")
	void OnAvailabilityChanged(bool bAvailable);

	/** Unspent points changed: a glow on the + buttons while there are points. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Attribute Panel")
	void OnUnspentPointsChanged(int32 Points);

private:
	UFUNCTION()
	void HandleAttributeChanged(FGameplayTag Attribute, float OldValue, float NewValue);

	UFUNCTION()
	void HandlePointsChanged(int32 UnspentPoints);

	UFUNCTION()
	void HandleEffectsChanged();

	void Bind();
	void Unbind();
	void BuildRows();
	void ClearRows();

	void RefreshRow(int32 EntryIndex);
	void RefreshCoreStats();
	void RefreshHeader();

	FText FormatValue(const FAttributeDisplayEntry& Entry, float Value) const;
	FText FormatEntry(const FAttributeDisplayEntry& Entry) const;
	FText FormatPreview(const FAttributeDisplayEntry& Entry) const;

	TWeakObjectPtr<AActor> ShownCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UAttributeComponent> Attributes;

	UPROPERTY(Transient)
	TObjectPtr<UCombatantComponent> Combatant;

	/** One per entry; null where the entry is hidden for this character. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAttributeRowWidget>> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCoreStatRowWidget>> CoreStatRows;

	/** Last shown text per row, to flash only real changes. */
	TArray<float> LastValues;

	FGameplayTag PreviewCoreStat;
};
