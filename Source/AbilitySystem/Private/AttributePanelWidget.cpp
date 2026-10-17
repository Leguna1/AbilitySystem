#include "AttributePanelWidget.h"

#include "AttributeComponent.h"
#include "AttributeRowWidget.h"
#include "CombatantComponent.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "CoreStatRowWidget.h"

#define LOCTEXT_NAMESPACE "AttributePanel"

void UAttributePanelWidget::SetCharacter(AActor* Character)
{
	// Same character, already shown: nothing to do. (Null always runs, so the grey-out event fires.)
	if (IsValid(Character) && Character == ShownCharacter.Get() && IsValid(Attributes))
	{
		return;
	}

	Unbind();
	ClearRows();

	ShownCharacter = Character;
	Attributes = IsValid(Character) ? Character->FindComponentByClass<UAttributeComponent>() : nullptr;
	Combatant = IsValid(Character) ? Character->FindComponentByClass<UCombatantComponent>() : nullptr;

	if (!IsValid(Attributes))
	{
		ShownCharacter.Reset();

		if (IsValid(LevelText))
		{
			LevelText->SetText(FText::GetEmpty());
		}

		if (IsValid(UnspentPointsText))
		{
			UnspentPointsText->SetText(FText::GetEmpty());
		}

		OnAvailabilityChanged(false);
		return;
	}

	Bind();
	BuildRows();
	RefreshAll();
	OnAvailabilityChanged(true);
}

void UAttributePanelWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UAttributePanelWidget::Bind()
{
	if (IsValid(Attributes))
	{
		Attributes->OnAttributeChanged.AddUniqueDynamic(this, &UAttributePanelWidget::HandleAttributeChanged);
		Attributes->OnAttributePointsChanged.AddUniqueDynamic(this, &UAttributePanelWidget::HandlePointsChanged);
	}

	// Damage reduction doesn't come from an attribute alone (caps, settings): refresh with effects too.
	if (IsValid(Combatant))
	{
		Combatant->OnEffectsChanged.AddUniqueDynamic(this, &UAttributePanelWidget::HandleEffectsChanged);
	}
}

void UAttributePanelWidget::Unbind()
{
	if (IsValid(Attributes))
	{
		Attributes->OnAttributeChanged.RemoveDynamic(this, &UAttributePanelWidget::HandleAttributeChanged);
		Attributes->OnAttributePointsChanged.RemoveDynamic(this, &UAttributePanelWidget::HandlePointsChanged);
	}

	if (IsValid(Combatant))
	{
		Combatant->OnEffectsChanged.RemoveDynamic(this, &UAttributePanelWidget::HandleEffectsChanged);
	}

	Attributes = nullptr;
	Combatant = nullptr;
}

void UAttributePanelWidget::BuildRows()
{
	ClearRows();

	Rows.SetNum(Entries.Num());
	LastValues.Init(TNumericLimits<float>::Lowest(), Entries.Num());

	if (IsValid(RowContainer) && RowClass)
	{
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const FAttributeDisplayEntry& Entry = Entries[Index];

			if (Entry.bHideIfMissing && !Attributes->HasAttribute(Entry.Attribute))
			{
				continue;
			}

			UAttributeRowWidget* Row = CreateWidget<UAttributeRowWidget>(this, RowClass);

			if (IsValid(Row))
			{
				Row->SetLabel(Entry.Label);
				Row->SetPreview(FText::GetEmpty());
				RowContainer->AddChild(Row);
				Rows[Index] = Row;
			}
		}
	}

	if (IsValid(CoreStatContainer) && CoreStatRowClass)
	{
		for (const FCoreStatDefinition& Definition : Attributes->GetCoreStats())
		{
			UCoreStatRowWidget* CoreRow = CreateWidget<UCoreStatRowWidget>(this, CoreStatRowClass);

			if (IsValid(CoreRow))
			{
				const FText Label = Definition.DisplayName.IsEmpty()
					? FText::FromName(Definition.CoreStat.GetTagName())
					: Definition.DisplayName;

				CoreRow->Setup(this, Definition.CoreStat, Label, Definition.Description);
				CoreStatContainer->AddChild(CoreRow);
				CoreStatRows.Add(CoreRow);
			}
		}
	}
}

void UAttributePanelWidget::ClearRows()
{
	for (UAttributeRowWidget* Row : Rows)
	{
		if (IsValid(Row))
		{
			Row->RemoveFromParent();
		}
	}

	for (UCoreStatRowWidget* CoreRow : CoreStatRows)
	{
		if (IsValid(CoreRow))
		{
			CoreRow->RemoveFromParent();
		}
	}

	Rows.Reset();
	CoreStatRows.Reset();
	LastValues.Reset();
	PreviewCoreStat = FGameplayTag();
}

void UAttributePanelWidget::RefreshAll()
{
	if (!IsValid(Attributes))
	{
		return;
	}

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		RefreshRow(Index);
	}

	RefreshCoreStats();
	RefreshHeader();
}

void UAttributePanelWidget::RefreshRow(const int32 EntryIndex)
{
	UAttributeRowWidget* Row = Rows.IsValidIndex(EntryIndex) ? Rows[EntryIndex].Get() : nullptr;

	if (!IsValid(Row) || !IsValid(Attributes))
	{
		return;
	}

	const FAttributeDisplayEntry& Entry = Entries[EntryIndex];

	// Compare the high end of a range (or the value itself) to decide the flash.
	const FGameplayTag Compared = Entry.RangeMaxAttribute.IsValid() ? Entry.RangeMaxAttribute : Entry.Attribute;
	const float Value = Attributes->GetAttributeValue(Compared);
	const float Previous = LastValues[EntryIndex];
	const bool bKnown = Previous != TNumericLimits<float>::Lowest();

	Row->SetValue(FormatEntry(Entry), bKnown && Value > Previous + KINDA_SMALL_NUMBER, bKnown && Value < Previous - KINDA_SMALL_NUMBER);
	Row->SetPreview(FormatPreview(Entry));
	LastValues[EntryIndex] = Value;
}

void UAttributePanelWidget::RefreshCoreStats()
{
	for (UCoreStatRowWidget* CoreRow : CoreStatRows)
	{
		if (IsValid(CoreRow))
		{
			const FGameplayTag CoreStat = CoreRow->GetCoreStat();
			CoreRow->Refresh(Attributes->GetAttributeValue(CoreStat), bAllowSpending && Attributes->CanSpendAttributePoints(CoreStat), bAllowSpending);
		}
	}
}

void UAttributePanelWidget::RefreshHeader()
{
	if (IsValid(LevelText))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Level"), Attributes->GetCharacterLevel());
		LevelText->SetText(FText::Format(LevelFormat, Args));
	}

	if (IsValid(UnspentPointsText))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Points"), Attributes->GetUnspentAttributePoints());
		UnspentPointsText->SetText(FText::Format(UnspentPointsFormat, Args));
		UnspentPointsText->SetVisibility(bAllowSpending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UAttributePanelWidget::SetPreviewCoreStat(const FGameplayTag CoreStat)
{
	if (PreviewCoreStat == CoreStat)
	{
		return;
	}

	PreviewCoreStat = CoreStat;

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (UAttributeRowWidget* Row = Rows[Index].Get())
		{
			Row->SetPreview(FormatPreview(Entries[Index]));
		}
	}
}

bool UAttributePanelWidget::SpendPoint(const FGameplayTag CoreStat)
{
	// Attribute changes refresh the rows on their own.
	return bAllowSpending && IsValid(Attributes) && Attributes->SpendAttributePoints(CoreStat, 1);
}

/* -------------------- Events -------------------- */

void UAttributePanelWidget::HandleAttributeChanged(const FGameplayTag Attribute, float OldValue, float NewValue)
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FAttributeDisplayEntry& Entry = Entries[Index];

		if (Entry.Attribute == Attribute || Entry.RangeMaxAttribute == Attribute || Entry.bShowDamageReduction)
		{
			RefreshRow(Index);
		}
	}

	// Core stats themselves are attributes; levels change spendability (max values).
	RefreshCoreStats();
	RefreshHeader();
}

void UAttributePanelWidget::HandlePointsChanged(const int32 UnspentPoints)
{
	RefreshCoreStats();
	RefreshHeader();
	OnUnspentPointsChanged(UnspentPoints);
}

void UAttributePanelWidget::HandleEffectsChanged()
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].bShowDamageReduction)
		{
			RefreshRow(Index);
		}
	}
}

/* -------------------- Formatting -------------------- */

FText UAttributePanelWidget::FormatValue(const FAttributeDisplayEntry& Entry, const float Value) const
{
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = Entry.Decimals;
	Options.MaximumFractionalDigits = Entry.Decimals;

	switch (Entry.Format)
	{
	case EAttributeDisplayFormat::Percent:
		return FText::Format(LOCTEXT("Percent", "{0}%"), FText::AsNumber(Value * 100.0f, &Options));

	case EAttributeDisplayFormat::BonusPercent:
	{
		const float Bonus = (Value - 1.0f) * 100.0f;
		return FText::Format(Bonus >= 0.0f ? LOCTEXT("BonusUp", "+{0}%") : LOCTEXT("BonusDown", "{0}%"), FText::AsNumber(Bonus, &Options));
	}

	default:
		return FText::AsNumber(Value, &Options);
	}
}

FText UAttributePanelWidget::FormatEntry(const FAttributeDisplayEntry& Entry) const
{
	const FText Low = FormatValue(Entry, Attributes->GetAttributeValue(Entry.Attribute));

	FText Shown = Entry.RangeMaxAttribute.IsValid()
		? FText::Format(LOCTEXT("Range", "{0} – {1}"), Low, FormatValue(Entry, Attributes->GetAttributeValue(Entry.RangeMaxAttribute)))
		: Low;

	if (Entry.bShowDamageReduction && IsValid(Combatant))
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 0;

		Shown = FText::Format(LOCTEXT("Reduction", "{0} ({1}%)"), Shown,
			FText::AsNumber(Combatant->GetDamageReduction(Entry.ReductionType) * 100.0f, &Options));
	}

	return Shown;
}

FText UAttributePanelWidget::FormatPreview(const FAttributeDisplayEntry& Entry) const
{
	if (!PreviewCoreStat.IsValid() || !IsValid(Attributes))
	{
		return FText::GetEmpty();
	}

	const float Gain = Attributes->GetContributionPreview(PreviewCoreStat, Entry.Attribute, 1);

	if (FMath::IsNearlyZero(Gain))
	{
		return FText::GetEmpty();
	}

	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 2;

	const FText Amount = Entry.Format == EAttributeDisplayFormat::Number
		? FText::AsNumber(Gain, &Options)
		: FText::Format(LOCTEXT("PreviewPercent", "{0}%"), FText::AsNumber(Gain * 100.0f, &Options));

	return FText::Format(Gain > 0.0f ? LOCTEXT("PreviewUp", "+{0}") : LOCTEXT("PreviewDown", "{0}"), Amount);
}

#undef LOCTEXT_NAMESPACE
