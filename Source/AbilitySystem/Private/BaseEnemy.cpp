#include "BaseEnemy.h"

#include "AbilityComponent.h"
#include "AttributeComponent.h"
#include "BrainComponent.h"
#include "CombatantComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnemyAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"
#include "ResourceComponent.h"
#include "Misc/DataValidation.h"
#include "Components/WidgetComponent.h"
#include "ResourceWidget.h"
#include "ActiveEffectsBarWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"


ABaseEnemy::ABaseEnemy()
{
	EnemyAbilityComponent = CreateDefaultSubobject<UAbilityComponent>(TEXT("AbilityComponent"));
	EnemyCombatantComponent = CreateDefaultSubobject<UCombatantComponent>(TEXT("CombatantComponent"));
	EnemyResourceComponent = CreateDefaultSubobject<UResourceComponent>(TEXT("ResourceComponent"));
	EnemyAttributeComponent = CreateDefaultSubobject<UAttributeComponent>(TEXT("AttributeComponent"));
	MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

	HealthBarComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarComponent"));
	HealthBarComponent->SetupAttachment(GetRootComponent());
	HealthBarComponent->SetWidgetSpace(EWidgetSpace::Screen); // always faces the camera, constant size on screen
	HealthBarComponent->SetDrawAtDesiredSize(true);
	HealthBarComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HealthBarComponent->SetGenerateOverlapEvents(false);
	
	TeamId = 1;

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// The controller's focus turns the enemy toward its target; movement turns it the way it walks.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;
}

void ABaseEnemy::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();

	if (UCombatantComponent* Combatant = GetCombatantComponent())
	{
		Combatant->OnPayloadReceived.AddDynamic(this, &ABaseEnemy::HandlePayloadReceived);
	}
	
	InitializeNameplate();
	SetNameplateVisible(false);
}

void ABaseEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCombatantComponent* Combatant = GetCombatantComponent())
	{
		Combatant->OnPayloadReceived.RemoveDynamic(this, &ABaseEnemy::HandlePayloadReceived);
	}

	Super::EndPlay(EndPlayReason);
}
void ABaseEnemy::InitializeNameplate()
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
	}
}
void ABaseEnemy::HandleCharacterDied(AActor* Killer)
{
	if (AEnemyAIController* EnemyController = Cast<AEnemyAIController>(GetController()))
	{
		EnemyController->ClearTarget();
		EnemyController->StopMovement();

		if (UBrainComponent* Brain = EnemyController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Died"));
		}
	}

	GetCharacterMovement()->DisableMovement();

	// The body no longer blocks the player or other enemies.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	if (CorpseLifetime > 0.0f)
	{
		SetLifeSpan(CorpseLifetime);
	}
	
	HealthBarComponent->SetVisibility(false);
	// Reaches OnCharacterDied for the death animation.
	Super::HandleCharacterDied(Killer);
}

void ABaseEnemy::HandlePayloadReceived(const FAbilityPayload& Payload, float DamageApplied)
{
	// Being shot from outside sight range still starts the fight.
	if (AEnemyAIController* EnemyController = Cast<AEnemyAIController>(GetController()))
	{
		EnemyController->NotifyAttackedBy(Payload.Instigator.Get());
	}
}
void ABaseEnemy::SetInCombat(const bool bNewInCombat)
{
	if (bInCombat == bNewInCombat)
	{
		return;
	}

	bInCombat = bNewInCombat;
	GetWorldTimerManager().ClearTimer(NameplateHideTimer);

	if (bInCombat)
	{
		SetNameplateVisible(true);
	}
	else if (NameplateHideDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(NameplateHideTimer,
			FTimerDelegate::CreateUObject(this, &ABaseEnemy::SetNameplateVisible, false),
			NameplateHideDelay, false);
	}
	else
	{
		SetNameplateVisible(false);
	}

	OnCombatStateChanged(bInCombat);
}

void ABaseEnemy::SetNameplateVisible(const bool bVisible)
{
	if (IsValid(HealthBarComponent))
	{
		HealthBarComponent->SetVisibility(bVisible);
	}
}

#if WITH_EDITOR
EDataValidationResult ABaseEnemy::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (int32 Index = 0; Index < Attacks.Num(); ++Index)
	{
		const FEnemyAttack& EnemyAttack = Attacks[Index];

		if (!EnemyAttack.AbilityTag.IsValid())
		{
			Context.AddError(FText::Format(NSLOCTEXT("Enemy", "NoAttackTag", "Attacks[{0}] has no Ability Tag."), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (EnemyAttack.MaxRange < EnemyAttack.MinRange)
		{
			Context.AddError(FText::Format(NSLOCTEXT("Enemy", "BadRange", "Attacks[{0}] Max Range is smaller than Min Range."), Index));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (AttackRecoveryMax < AttackRecoveryMin)
	{
		Context.AddError(NSLOCTEXT("Enemy", "BadRecovery", "Attack Recovery Max is smaller than Min."));
		Result = EDataValidationResult::Invalid;
	}

	if (RoamWaitMax < RoamWaitMin)
	{
		Context.AddError(NSLOCTEXT("Enemy", "BadRoamWait", "Roam Wait Max is smaller than Min."));
		Result = EDataValidationResult::Invalid;
	}
	
	const bool bAnyChases = Attacks.ContainsByPredicate([](const FEnemyAttack& EnemyAttack)
	
	{
	return EnemyAttack.bChaseIntoRange;
});

	if (!Attacks.IsEmpty() && !bAnyChases)
	{
		Context.AddWarning(NSLOCTEXT("Enemy", "NeverChases",
			"No attack has Chase Into Range on, so this enemy never approaches its target."));
	}
	
	return Result;
}
#endif