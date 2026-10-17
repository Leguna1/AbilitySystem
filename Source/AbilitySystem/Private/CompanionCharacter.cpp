#include "CompanionCharacter.h"

#include "ActionBarComponent.h"
#include "CombatantComponent.h"
#include "CompanionAIController.h"
#include "ExperienceComponent.h"

ACompanionCharacter::ACompanionCharacter()
{
	TeamId = 0;
	AIControllerClass = ACompanionAIController::StaticClass();

	// Spawned by an ability, never placed: possess as soon as it's spawned.
	AutoPossessAI = EAutoPossessAI::Spawned;

	// The player wants to see their pet's health.
	bAlwaysShowNameplate = true;

	CompanionActionBar = CreateDefaultSubobject<UActionBarComponent>(TEXT("CompanionActionBar"));
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
