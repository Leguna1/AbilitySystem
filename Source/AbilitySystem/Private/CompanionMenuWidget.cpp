#include "CompanionMenuWidget.h"

#include "AbilityBarWidget.h"
#include "AbilityComponent.h"
#include "AttributeComponent.h"
#include "AttributePanelWidget.h"
#include "CharacterMenuWidget.h"
#include "CombatantComponent.h"
#include "CompanionCharacter.h"
#include "CompanionComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "ExperienceComponent.h"
#include "GameFramework/Pawn.h"
#include "PoiseBarWidget.h"
#include "ResourceComponent.h"
#include "ResourceWidget.h"

namespace CompanionMenu
{
	/** Same source as the player bar's pet level: Experience Component, else the Attribute Component's level. */
	int32 GetActorLevel(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return 0;
		}

		if (const UExperienceComponent* Experience = Actor->FindComponentByClass<UExperienceComponent>())
		{
			return Experience->GetLevel();
		}

		const UAttributeComponent* Attributes = Actor->FindComponentByClass<UAttributeComponent>();
		return IsValid(Attributes) ? Attributes->GetCharacterLevel() : 0;
	}
}

/* -------------------- Lifetime -------------------- */

void UCompanionMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Starts empty until a leader with a companion is known.
	ShowCompanion(nullptr);
	SetLeader(GetOwningPlayerPawn());
}

void UCompanionMenuWidget::NativeDestruct()
{
	UnbindLeader();
	Super::NativeDestruct();
}

/* -------------------- Leader -------------------- */

void UCompanionMenuWidget::SetLeader(APawn* Leader)
{
	UCompanionComponent* Companions = IsValid(Leader) ? Leader->FindComponentByClass<UCompanionComponent>() : nullptr;

	if (Companions != BoundCompanions.Get())
	{
		UnbindLeader();

		if (IsValid(Companions))
		{
			Companions->OnCompanionChanged.AddUniqueDynamic(this, &UCompanionMenuWidget::HandleCompanionChanged);
			BoundCompanions = Companions;
		}
	}

	ShowCompanion(IsValid(Companions) ? Companions->GetCompanion() : nullptr);
}

void UCompanionMenuWidget::UnbindLeader()
{
	if (UCompanionComponent* Companions = BoundCompanions.Get())
	{
		Companions->OnCompanionChanged.RemoveDynamic(this, &UCompanionMenuWidget::HandleCompanionChanged);
	}

	BoundCompanions.Reset();
}

void UCompanionMenuWidget::HandleCompanionChanged(ACompanionCharacter* Companion)
{
	ShowCompanion(Companion);

	// The preview follows too, if this tab is open (summoned, died or dismissed while the menu is up).
	if (UCharacterMenuWidget* Menu = GetTypedOuter<UCharacterMenuWidget>())
	{
		Menu->RefreshPreview();
	}
}

/* -------------------- Showing -------------------- */

void UCompanionMenuWidget::ShowCompanion(ACompanionCharacter* Companion)
{
	const bool bHasCompanion = IsValid(Companion);
	const bool bChanged = Companion != ShownCompanion.Get();
	ShownCompanion = Companion;

	if (IsValid(ContentPanel))
	{
		ContentPanel->SetVisibility(bHasCompanion ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (IsValid(EmptyStatePanel))
	{
		EmptyStatePanel->SetVisibility(bHasCompanion ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	// Null clears each part, so nothing keeps showing (or holding on to) a dead companion.
	if (IsValid(HealthBar))
	{
		HealthBar->SetResourceComponent(bHasCompanion ? Companion->FindComponentByClass<UResourceComponent>() : nullptr);
	}

	if (IsValid(PoiseBar))
	{
		PoiseBar->SetCombatantComponent(bHasCompanion ? Companion->FindComponentByClass<UCombatantComponent>() : nullptr);
	}

	if (IsValid(AbilityBar))
	{
		AbilityBar->InitializeBar(bHasCompanion ? Companion->FindComponentByClass<UAbilityComponent>() : nullptr);
	}

	if (IsValid(CompanionStats))
	{
		CompanionStats->SetCharacter(Companion);
	}

	RefreshCompanion();

	if (bChanged)
	{
		OnCompanionShown(Companion);
	}
}

void UCompanionMenuWidget::RefreshCompanion()
{
	const ACompanionCharacter* Companion = ShownCompanion.Get();

	if (!IsValid(Companion))
	{
		return;
	}

	if (IsValid(NameText))
	{
		NameText->SetText(Companion->GetCharacterDisplayName());
	}

	if (IsValid(LevelText))
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Level"), CompanionMenu::GetActorLevel(Companion));
		LevelText->SetText(FText::Format(LevelFormat, Arguments));
	}

	if (IsValid(Portrait) && IsValid(Companion->GetCharacterPortrait()))
	{
		Portrait->SetBrushFromTexture(Companion->GetCharacterPortrait());
	}
}
