#include "CompanionCharacter.h"

#include "ActionBarComponent.h"
#include "CombatantComponent.h"
#include "CompanionAIController.h"
#include "CompanionNeedsComponent.h"
#include "ExperienceComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

ACompanionCharacter::ACompanionCharacter()
{
	TeamId = 0;
	AIControllerClass = ACompanionAIController::StaticClass();

	// Spawned by an ability, never placed: possess as soon as it's spawned.
	AutoPossessAI = EAutoPossessAI::Spawned;

	// The player wants to see their pet's health.
	bAlwaysShowNameplate = true;

	CompanionActionBar = CreateDefaultSubobject<UActionBarComponent>(TEXT("CompanionActionBar"));
	NeedsComponent = CreateDefaultSubobject<UCompanionNeedsComponent>(TEXT("NeedsComponent"));
}

void ACompanionCharacter::SetLeader(ABaseCharacter* NewLeader)
{
	Leader = NewLeader;

	if (IsValid(NewLeader))
	{
		TeamId = NewLeader->GetGenericTeamId().GetId();
	}
}

void ACompanionCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCombatantComponent* Combatant = GetCombatantComponent())
	{
		Combatant->OnKilled.AddDynamic(this, &ACompanionCharacter::HandleKilled);
	}
}

void ACompanionCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UCombatantComponent* Combatant = GetCombatantComponent())
	{
		Combatant->OnKilled.RemoveDynamic(this, &ACompanionCharacter::HandleKilled);
	}

	Super::EndPlay(EndPlayReason);
}

void ACompanionCharacter::HandleKilled(AActor* Victim)
{
	if (!bShareKillsWithLeader || !Leader.IsValid())
	{
		return;
	}

	if (UExperienceComponent* LeaderExperience = Leader->FindComponentByClass<UExperienceComponent>())
	{
		LeaderExperience->AwardKill(Victim);
	}
}

#if WITH_EDITOR
EDataValidationResult ACompanionCharacter::IsDataValid(FDataValidationContext& Context) const
{
	// Skips Base AI Character's Attacks checks on purpose: companions don't use that list.
	EDataValidationResult Result = ABaseCharacter::IsDataValid(Context);

	if (Tactics.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("Companion", "NoTactics", "No Tactics: this companion follows its leader but never fights."));
	}

	if (!Attacks.IsEmpty())
	{
		Context.AddWarning(NSLOCTEXT("Companion", "AttacksUnused", "Attacks is not used by companions; teach it through Tactics instead."));
	}

	for (int32 Index = 0; Index < Tactics.Num(); ++Index)
	{
		const FCompanionTactic& Tactic = Tactics[Index];

		if (!Tactic.AbilityTag.IsValid())
		{
			Context.AddError(FText::Format(NSLOCTEXT("Companion", "NoTacticTag", "Tactics[{0}] has no Ability Tag."), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (Tactic.MaxRange < Tactic.MinRange)
		{
			Context.AddError(FText::Format(NSLOCTEXT("Companion", "BadTacticRange", "Tactics[{0}] Max Range is smaller than Min Range."), Index));
			Result = EDataValidationResult::Invalid;
		}

		if (Tactic.Considerations.Contains(nullptr))
		{
			Context.AddWarning(FText::Format(NSLOCTEXT("Companion", "EmptyConsideration", "Tactics[{0}] has an empty Considerations entry."), Index));
		}
	}

	return Result;
}
#endif
