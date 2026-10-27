// PlayerBarWidget.cpp
#include "PlayerBarWidget.h"

#include "AbilityBarWidget.h"
#include "AbilityComponent.h"
#include "AbilityProgressWidget.h"
#include "AttributeComponent.h"
#include "BaseCharacter.h"
#include "CombatantComponent.h"
#include "CompanionCharacter.h"
#include "CompanionComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "ExperienceComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PoiseBarWidget.h"
#include "ResourceComponent.h"
#include "ResourceWidget.h"

namespace
{
	/** Name and portrait from the character base, if it is one. */
	void ApplyCharacterInfo(const AActor* Character, UTextBlock* NameText, UImage* PortraitImage)
	{
		const ABaseCharacter* BaseCharacter = Cast<ABaseCharacter>(Character);

		if (IsValid(NameText))
		{
			NameText->SetText(IsValid(BaseCharacter) ? BaseCharacter->GetCharacterDisplayName() : FText::GetEmpty());
		}

		if (IsValid(PortraitImage) && IsValid(BaseCharacter) && IsValid(BaseCharacter->GetCharacterPortrait()))
		{
			PortraitImage->SetBrushFromTexture(BaseCharacter->GetCharacterPortrait());
		}
	}

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

void UPlayerBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// XP only ever needs its current value.
	FUIBarMaterialSettings Settings = XPBarSettings;
	Settings.TrailDuration = 0.0f;
	XPBar.Initialize(XPFill, Settings);

	if (IsValid(MedallionGlow))
	{
		GlowMaterial = MedallionGlow->GetDynamicMaterial();

		if (IsValid(GlowMaterial))
		{
			GlowMaterial->SetScalarParameterValue(GlowIntensityParameter, 0.0f);
		}
	}

	// No pet until one shows up: panel tucked away, socket closed.
	PetSlideAlpha = 0.0f;
	PetSlideTarget = 0.0f;
	ApplyPetSlide(0.0f);

	if (IsValid(PetPanel))
	{
		PetPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (IsValid(SocketGem))
	{
		SocketGem->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->OnPossessedPawnChanged.AddUniqueDynamic(this, &UPlayerBarWidget::HandlePossessedPawnChanged);
		SetPawn(PlayerController->GetPawn());
	}
}

void UPlayerBarWidget::NativeDestruct()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->OnPossessedPawnChanged.RemoveDynamic(this, &UPlayerBarWidget::HandlePossessedPawnChanged);
	}

	UnbindPawn();
	Super::NativeDestruct();
}

void UPlayerBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	TickLevelGlow(InDeltaTime);
	TickPetSlide(InDeltaTime);
}

void UPlayerBarWidget::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	SetPawn(NewPawn);
}

/* -------------------- Pawn -------------------- */

void UPlayerBarWidget::SetPawn(APawn* NewPawn)
{
	if (NewPawn == BoundPawn)
	{
		return;
	}

	UnbindPawn();
	BindPawn(NewPawn);
}

void UPlayerBarWidget::BindPawn(APawn* NewPawn)
{
	BoundPawn = NewPawn;

	if (!IsValid(BoundPawn))
	{
		return;
	}

	UAbilityComponent* AbilityComponent = BoundPawn->FindComponentByClass<UAbilityComponent>();
	UResourceComponent* Resources = BoundPawn->FindComponentByClass<UResourceComponent>();
	UCombatantComponent* Combatant = BoundPawn->FindComponentByClass<UCombatantComponent>();

	for (UAbilityBarWidget* Bar : { LeftSlots.Get(), PodSlots.Get(), RightSlots.Get() })
	{
		if (IsValid(Bar))
		{
			Bar->InitializeBar(AbilityComponent);
		}
	}

	if (IsValid(CastBar))
	{
		CastBar->InitializeProgress(AbilityComponent);
	}

	if (IsValid(HealthBar))
	{
		HealthBar->SetResourceComponent(Resources);
	}

	if (IsValid(FocusBar))
	{
		FocusBar->SetResourceComponent(Resources);
	}

	if (IsValid(StaggerBar))
	{
		StaggerBar->SetCombatantComponent(Combatant);
	}

	ApplyCharacterInfo(BoundPawn, PlayerNameText, PlayerPortrait);

	ExperienceComponent = BoundPawn->FindComponentByClass<UExperienceComponent>();

	if (IsValid(ExperienceComponent))
	{
		ExperienceComponent->OnExperienceChanged.AddUniqueDynamic(this, &UPlayerBarWidget::HandleExperienceChanged);
		ExperienceComponent->OnLevelUp.AddUniqueDynamic(this, &UPlayerBarWidget::HandleLevelUp);
	}

	RefreshExperience();

	Companions = BoundPawn->FindComponentByClass<UCompanionComponent>();

	if (IsValid(Companions))
	{
		Companions->OnCompanionChanged.AddUniqueDynamic(this, &UPlayerBarWidget::HandleCompanionChanged);
	}

	ShowCompanion(IsValid(Companions) ? Companions->GetCompanion() : nullptr);

	// A pet already out when the bar appears is simply there, no slide.
	PetSlideAlpha = PetSlideTarget;
	ApplyPetSlide(PetSlideAlpha);
}

void UPlayerBarWidget::UnbindPawn()
{
	if (IsValid(ExperienceComponent))
	{
		ExperienceComponent->OnExperienceChanged.RemoveDynamic(this, &UPlayerBarWidget::HandleExperienceChanged);
		ExperienceComponent->OnLevelUp.RemoveDynamic(this, &UPlayerBarWidget::HandleLevelUp);
	}

	if (IsValid(Companions))
	{
		Companions->OnCompanionChanged.RemoveDynamic(this, &UPlayerBarWidget::HandleCompanionChanged);
	}

	ExperienceComponent = nullptr;
	Companions = nullptr;
	BoundPawn = nullptr;
}

/* -------------------- Level and XP -------------------- */

void UPlayerBarWidget::HandleExperienceChanged(const int32 Experience, const int32 ExperienceToNextLevel, const int32 Level)
{
	RefreshExperience();
}

void UPlayerBarWidget::HandleLevelUp(const int32 NewLevel)
{
	GlowRemaining = LevelUpGlowDuration;
	TickLevelGlow(0.0f);

	RefreshExperience();

	// The companion's level follows the leader's.
	RefreshCompanionInfo();

	OnLevelUp(NewLevel);
}

void UPlayerBarWidget::RefreshExperience()
{
	const float Progress = IsValid(ExperienceComponent) ? FMath::Clamp(ExperienceComponent->GetLevelProgress(), 0.0f, 1.0f) : 0.0f;

	XPBar.SetPercent(Progress, true);

	if (IsValid(XPText))
	{
		XPText->SetText(FText::FromString(FString::Printf(TEXT("%d %%"), FMath::FloorToInt(Progress * 100.0f))));
	}

	if (IsValid(LevelText))
	{
		LevelText->SetText(FText::AsNumber(GetActorLevel(BoundPawn)));
	}
}

void UPlayerBarWidget::TickLevelGlow(const float DeltaTime)
{
	if (GlowRemaining <= 0.0f && DeltaTime > 0.0f)
	{
		return;
	}

	GlowRemaining = FMath::Max(GlowRemaining - DeltaTime, 0.0f);

	if (IsValid(GlowMaterial))
	{
		const float Intensity = LevelUpGlowDuration > 0.0f ? GlowRemaining / LevelUpGlowDuration : 0.0f;
		GlowMaterial->SetScalarParameterValue(GlowIntensityParameter, Intensity);
	}
}

/* -------------------- Companion -------------------- */

void UPlayerBarWidget::HandleCompanionChanged(ACompanionCharacter* Companion)
{
	ShowCompanion(Companion);
	OnCompanionShown(Companion);
}

void UPlayerBarWidget::ShowCompanion(ACompanionCharacter* Companion)
{
	ShownCompanion = Companion;

	if (!IsValid(Companion))
	{
		// Slides back in; the parts are cleared once it is tucked away (Tick Pet Slide).
		PetSlideTarget = 0.0f;
		return;
	}

	if (IsValid(PetHealthBar))
	{
		PetHealthBar->SetResourceComponent(Companion->FindComponentByClass<UResourceComponent>());
	}

	if (IsValid(PetStaggerBar))
	{
		PetStaggerBar->SetCombatantComponent(Companion->FindComponentByClass<UCombatantComponent>());
	}

	if (IsValid(PetSlots))
	{
		PetSlots->InitializeBar(Companion->FindComponentByClass<UAbilityComponent>());
	}

	RefreshCompanionInfo();

	if (IsValid(SocketGem))
	{
		SocketGem->SetVisibility(ESlateVisibility::Hidden);
	}

	if (IsValid(PetPanel))
	{
		PetPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	PetSlideTarget = 1.0f;
}

void UPlayerBarWidget::RefreshCompanionInfo()
{
	if (!IsValid(ShownCompanion))
	{
		return;
	}

	ApplyCharacterInfo(ShownCompanion, PetNameText, PetPortrait);

	if (IsValid(PetLevelText))
	{
		PetLevelText->SetText(FText::AsNumber(GetActorLevel(ShownCompanion)));
	}
}

void UPlayerBarWidget::TickPetSlide(const float DeltaTime)
{
	if (FMath::IsNearlyEqual(PetSlideAlpha, PetSlideTarget))
	{
		return;
	}

	const float Step = PetSlideDuration > 0.0f ? DeltaTime / PetSlideDuration : 1.0f;
	PetSlideAlpha = PetSlideTarget > PetSlideAlpha
		? FMath::Min(PetSlideAlpha + Step, PetSlideTarget)
		: FMath::Max(PetSlideAlpha - Step, PetSlideTarget);

	ApplyPetSlide(PetSlideAlpha);

	// Tucked away: hide the panel, close the socket, let go of the old companion.
	if (PetSlideAlpha <= 0.0f && PetSlideTarget <= 0.0f)
	{
		if (IsValid(PetPanel))
		{
			PetPanel->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (IsValid(SocketGem))
		{
			SocketGem->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		if (IsValid(PetHealthBar))
		{
			PetHealthBar->SetResourceComponent(nullptr);
		}

		if (IsValid(PetStaggerBar))
		{
			PetStaggerBar->SetCombatantComponent(nullptr);
		}

		if (IsValid(PetSlots))
		{
			PetSlots->InitializeBar(nullptr);
		}
	}
}

void UPlayerBarWidget::ApplyPetSlide(const float Alpha)
{
	if (!IsValid(PetPanel))
	{
		return;
	}

	const float Eased = FMath::InterpEaseOut(0.0f, 1.0f, FMath::Clamp(Alpha, 0.0f, 1.0f), 2.0f);

	PetPanel->SetRenderTranslation(FVector2D(PetSlideOffset * (1.0f - Eased), 0.0f));
	PetPanel->SetRenderOpacity(Eased);
}
