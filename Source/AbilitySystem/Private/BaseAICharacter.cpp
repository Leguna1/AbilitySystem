#include "BaseAICharacter.h"

#include "AbilityComponent.h"
#include "ActiveEffectsBarWidget.h"
#include "AttributeComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "CombatAIController.h"
#include "CombatantComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "PoiseBarWidget.h"
#include "ResourceComponent.h"
#include "ResourceWidget.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ABaseAICharacter::ABaseAICharacter()
{
	// Same subobject names as when these lived on Base Enemy, so existing Blueprints keep their settings.
	AIAbilityComponent = CreateDefaultSubobject<UAbilityComponent>(TEXT("AbilityComponent"));
	AICombatantComponent = CreateDefaultSubobject<UCombatantComponent>(TEXT("CombatantComponent"));
	AIResourceComponent = CreateDefaultSubobject<UResourceComponent>(TEXT("ResourceComponent"));
	AIAttributeComponent = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarComponent"));
	HealthBarComponent->SetupAttachment(GetRootComponent());
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen); // always faces the camera, constant size on screen
	HealthBarComponent->SetDrawAtDesiredSize(true);
	HealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarComponent->SetGenerateOverlapEvents(false);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// The controller's focus turns the character toward its target; movement turns it the way it walks.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void ABaseAICharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCombatantComponent* Combatant = GetCombatantComponent())
	{
		Combatant->OnPayloadReceived.AddDynamic(this, &ABaseAICharacter::HandlePayloadReceived);
	}

	InitializeNameplate();
	SetNameplateVisible(bAlwaysShowNameplate);
}

void ABaseAICharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCombatantComponent* Combatant = GetCombatantComponent())
	{
		Combatant->OnPayloadReceived.RemoveDynamic(this, &ABaseAICharacter::HandlePayloadReceived);
	}

	Super::EndPlay(EndPlayReason);
}

void ABaseAICharacter::InitializeNameplate()
{
	UUserWidget* Nameplate = IsValid(HealthBarComponent) ? HealthBarComponent->GetUserWidgetObject() : nullptr;

	if (!IsValid(Nameplate) || !IsValid(Nameplate->WidgetTree))
	{
		return;
	}

	TArray<UWidget*> NameplateWidgets;
	Nameplate->WidgetTree->GetAllWidgets(NameplateWidgets);
	NameplateWidgets.Add(Nameplate);

	for (UWidget* NameplateWidget : NameplateWidgets)
	{
		if (UResourceWidget* ResourceBar = Cast<UResourceWidget>(NameplateWidget))
		{
			ResourceBar->SetResourceComponent(GetResourceComponent());
		}
		else if (UActiveEffectsBarWidget* EffectsBar = Cast<UActiveEffectsBarWidget>(NameplateWidget))
		{
			EffectsBar->InitializeEffectsBar(GetCombatantComponent());
		}
		else if (UPoiseBarWidget* PoiseBar = Cast<UPoiseBarWidget>(NameplateWidget))
		{
			PoiseBar->SetCombatantComponent(GetCombatantComponent());
		}
	}
}

void ABaseAICharacter::HandleCharacterDied(AActor* Killer)
{
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (ACombatAIController* CombatController = Cast<ACombatAIController>(AIController))
		{
			CombatController->ClearTarget();
		}

		AIController->StopMovement();

		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Died"));
		}
	}

	GetCharacterMovement()->DisableMovement();

	// The body no longer blocks anyone.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	if (CorpseLifetime > 0.0f)
	{
		SetLifeSpan(CorpseLifetime);
	}

	GetWorldTimerManager().ClearTimer(NameplateHideTimer);
	SetNameplateVisible(false);

	// Death montage and sound, then OnCharacterDied.
	Super::HandleCharacterDied(Killer);
}

void ABaseAICharacter::HandlePayloadReceived(const FAbilityPayload& Payload, float DamageApplied)
{
	// Being hit from outside sight range still starts the fight.
	if (ACombatAIController* CombatController = Cast<ACombatAIController>(GetController()))
	{
		CombatController->NotifyAttackedBy(Payload.Instigator.Get());
	}
}

void ABaseAICharacter::SetInCombat(const bool bNewInCombat)
{
	if (bInCombat == bNewInCombat)
	{
		return;
	}

	bInCombat = bNewInCombat;
	GetWorldTimerManager().ClearTimer(NameplateHideTimer);

	if (bInCombat || bAlwaysShowNameplate)
	{
		SetNameplateVisible(true);
	}
	else if (NameplateHideDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(NameplateHideTimer,
			FTimerDelegate::CreateUObject(this, &ABaseAICharacter::SetNameplateVisible, false),
			NameplateHideDelay, false);
	}
	else
	{
		SetNameplateVisible(false);
	}

	OnCombatStateChanged(bInCombat);
}

void ABaseAICharacter::SetNameplateVisible(const bool bVisible)
{
	if (IsValid(HealthBarComponent))
	{
		HealthBarComponent->SetVisibility(bVisible);
	}
}

#if WITH_EDITOR
EDataValidationResult ABaseAICharacter::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		const FAIAttack& Attack = Attacks[Index];

		if (!Attack.AbilityTag.IsValid())
		{
			Context.AddError(FText::Format(NSLOCTEXT("AICharacter", "NoAttackTag", "Attacks[{0}] has no Ability Tag."), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (Attack.MaxRange < Attack.MinRange)
		{
			Context.AddError(FText::Format(NSLOCTEXT("AICharacter", "BadRange", "Attacks[{0}] Max Range is smaller than Min Range."), Index));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (AttackRecoveryMax < AttackRecoveryMin)
	{
		Context.AddError(NSLOCTEXT("AICharacter", "BadRecovery", "Attack Recovery Max is smaller than Min."));
		Result = EDataValidationResult::Invalid;
	}

	const bool bAnyChases = Attacks.ContainsByPredicate([](const FAIAttack& Attack)
	{
		return Attack.bChaseIntoRange;
	});

	if (!Attacks.IsEmpty() && !bAnyChases)
	{
		Context.AddWarning(NSLOCTEXT("AICharacter", "NeverChases",
			"No attack has Chase Into Range on, so this character never approaches its target."));
	}

	return Result;
}
#endif
