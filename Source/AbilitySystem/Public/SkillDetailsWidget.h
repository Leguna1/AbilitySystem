#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ability.h"
#include "GameplayTagContainer.h"
#include "SkillTreeAsset.h"
#include "SkillTreeComponent.h"
#include "SkillDetailsWidget.generated.h"

class UImage;
class UPanelWidget;
class URichTextBlock;
class UTextBlock;
class UTexture2D;

/** Everything the details panel shows for one node. */
USTRUCT(BlueprintType)
struct FSkillNodeDetails
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	FGameplayTag NodeId;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	ESkillNodeKind Kind = ESkillNodeKind::Skill;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	ESkillNodeState State = ESkillNodeState::Locked;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	int32 Rank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	int32 MaxRank = 0;

	/** Learned: what the current rank does. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	bool bShowCurrent = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	FText CurrentDescription;

	/** Cost, cooldown, damage type... at the current rank. Empty for most passives. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	TArray<FAbilityDisplayStat> CurrentStats;

	/** Not maxed: what the next rank does. Changed numbers are wrapped in <Changed></> when highlighting is on. */
	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	bool bShowNext = false;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	FText NextDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Skill Details")
	TArray<FAbilityDisplayStat> NextStats;
};

/**
 * A fixed panel describing one skill tree node: its current rank and next rank side by side.
 * Unlearned shows only the next rank; maxed shows only the current one.
 *
 * The skill tree fills it when a node is hovered (place one named "DetailsPanel" in the tree widget,
 * or call Set Details Widget on the tree). It refreshes itself when ranks change.
 *
 * Optional widgets in the UMG subclass, found by name:
 *  - NameText (Text), IconImage (Image)
 *  - CurrentColumn (any panel), CurrentHeaderText (Text), CurrentStatsText (Rich Text), CurrentDescriptionText (Rich Text)
 *  - NextColumn (any panel), NextHeaderText (Text), NextStatsText (Rich Text), NextDescriptionText (Rich Text)
 *  - DetailsContent: shown while a node is displayed; EmptyHint: shown while none is
 * Give the Rich Text Blocks a style row named "Changed" to color the numbers that improve.
 * Or skip them all and build the look in On Details Updated.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API USkillDetailsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Shows NodeId of TreeComponent's tree. */
	UFUNCTION(BlueprintCallable, Category = "Skill Details")
	void ShowNode(USkillTreeComponent* InTreeComponent, FGameplayTag InNodeId);

	/** Re-reads the shown node, e.g. after a rank was bought. */
	UFUNCTION(BlueprintCallable, Category = "Skill Details")
	void Refresh();

	/** Shows nothing (the Empty Hint, if any). */
	UFUNCTION(BlueprintCallable, Category = "Skill Details")
	void ClearDetails();

	UFUNCTION(BlueprintPure, Category = "Skill Details")
	FGameplayTag GetShownNodeId() const { return NodeId; }

protected:
	virtual void NativeConstruct() override;

	/** A node is shown or changed. Bound widgets are already filled; use this for anything extra. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Details")
	void OnDetailsUpdated(const FSkillNodeDetails& Details);

	UFUNCTION(BlueprintImplementableEvent, Category = "Skill Details")
	void OnDetailsCleared();

	/** Header of the current-rank column. {Rank} and {MaxRank} are filled in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Details")
	FText CurrentHeaderFormat = NSLOCTEXT("SkillDetails", "CurrentHeader", "Current Rank ({Rank}/{MaxRank})");

	/** Header of the next-rank column. {Rank} (the next one) and {MaxRank} are filled in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Details")
	FText NextHeaderFormat = NSLOCTEXT("SkillDetails", "NextHeader", "Next Rank ({Rank}/{MaxRank})");

	/** Header when the node isn't learned yet: the first rank is what you'd buy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Details")
	FText UnlearnedHeaderFormat = NSLOCTEXT("SkillDetails", "UnlearnedHeader", "Rank 1/{MaxRank}");

	/** One stat in the stat block. {Label} and {Value} are filled in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Details")
	FText StatFormat = NSLOCTEXT("SkillDetails", "StatFormat", "{Label}: {Value}");

	/** Between stats in the stat block. Use a line break (Shift+Enter) for one stat per line. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Details", meta = (MultiLine = "true"))
	FText StatSeparator = INVTEXT("   ");

	/** Wrap the next rank's changed numbers in <Changed></> for the Rich Text style. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Details")
	bool bHighlightChanges = true;

	/* -------------------- Optional bound widgets -------------------- */

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<UImage> IconImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<UPanelWidget> CurrentColumn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<UTextBlock> CurrentHeaderText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<URichTextBlock> CurrentStatsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<URichTextBlock> CurrentDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<UPanelWidget> NextColumn;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<UTextBlock> NextHeaderText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<URichTextBlock> NextStatsText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<URichTextBlock> NextDescriptionText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<UWidget> DetailsContent;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Skill Details")
	TObjectPtr<UWidget> EmptyHint;

private:
	bool BuildDetails(FSkillNodeDetails& OutDetails) const;
	void ApplyToBoundWidgets(const FSkillNodeDetails& Details);

	/** Stats as one line of text. Values that differ from CompareStats (matched by label) are wrapped in <Changed></>. */
	FText FormatStats(const TArray<FAbilityDisplayStat>& Stats, const TArray<FAbilityDisplayStat>* CompareStats) const;
	void SetShowingNode(bool bShowingNode);

	UPROPERTY(Transient)
	TObjectPtr<USkillTreeComponent> TreeComponent;

	FGameplayTag NodeId;
};
