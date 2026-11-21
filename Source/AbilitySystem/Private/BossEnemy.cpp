#include "BossEnemy.h"

#include "AdventureComponent.h"
#include "CombatantComponent.h"
#include "TimerManager.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "BossEnemy"

ABossEnemy::ABossEnemy()
{
	// A boss fight shouldn't end because the player backed off a little; the body stays to be looked at.
	LeashRadius = 4000.0f;
	AggroRadius = 1800.0f;
	CorpseLifetime = 20.0f;
	ExperienceReward = 1000;
	RoamRadius = 0.0f;
}

void ABossEnemy::BeginPlay()
{
	Super::BeginPlay();

	UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this);

	if (IsValid(Progress))
	{
		Progress->RegisterObjective(BossId, GetCharacterDisplayName(), true);

		// Killed in an earlier session: it still counts as an objective, but it isn't here anymore.
		if (Progress->HasFlag(BossId))
		{
			Destroy();
			return;
		}
	}

	PhaseStarted.Init(false, Phases.Num());

	if (UResourceComponent* Resources = GetResourceComponent())
	{
		Resources->OnResourceChanged.AddUniqueDynamic(this, &ABossEnemy::HandleResourceChanged);
	}

	GetWorldTimerManager().SetTimer(BossBarTimer, this, &ABossEnemy::UpdateBossBar, 0.25f, true);
}

void ABossEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(BossBarTimer);

	if (UResourceComponent* Resources = GetResourceComponent())
	{
		Resources->OnResourceChanged.RemoveDynamic(this, &ABossEnemy::HandleResourceChanged);
	}

	if (bBossBarShown)
	{
		if (UAdventureComponent* Adventure = UAdventureComponent::Get(this))
		{
			if (Adventure->GetActiveBoss() == this)
			{
				Adventure->SetActiveBoss(nullptr);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ABossEnemy::UpdateBossBar()
{
	const bool bFighting = IsInCombat() && !IsDead();

	if (bFighting == bBossBarShown)
	{
		return;
	}

	UAdventureComponent* Adventure = UAdventureComponent::Get(this);

	if (!IsValid(Adventure))
	{
		return;
	}

	bBossBarShown = bFighting;

	if (bFighting)
	{
		Adventure->SetActiveBoss(this);
	}
	else if (Adventure->GetActiveBoss() == this)
	{
		Adventure->SetActiveBoss(nullptr);
	}
}

/* -------------------- Phases -------------------- */

void ABossEnemy::HandleResourceChanged(EResourceType ResourceType, EResourceValueType ValueType, float OldValue, float NewValue)
{
	if (ResourceType != EResourceType::Health || ValueType != EResourceValueType::Current || NewValue <= 0.0f || NewValue >= OldValue)
	{
		return;
	}

	const UResourceComponent* Resources = GetResourceComponent();
	const float MaxHealth = IsValid(Resources) ? Resources->GetResourceValue(EResourceType::Health, EResourceValueType::Max) : 0.0f;

	if (MaxHealth <= 0.0f)
	{
		return;
	}

	const float Fraction = NewValue / MaxHealth;

	for (int32 PhaseIndex = 0; PhaseIndex < Phases.Num(); ++PhaseIndex)
	{
		if (PhaseStarted.IsValidIndex(PhaseIndex) && !PhaseStarted[PhaseIndex] && Fraction <= Phases[PhaseIndex].HealthFraction)
		{
			StartPhase(PhaseIndex);
		}
	}
}

void ABossEnemy::StartPhase(const int32 PhaseIndex)
{
	const FBossPhase& Phase = Phases[PhaseIndex];

	PhaseStarted[PhaseIndex] = true;
	CurrentPhase = PhaseIndex;

	if (UCombatantComponent* Combatant = GetCombatantComponent())
	{
		if (!Phase.Modifiers.IsEmpty())
		{
			// Keyed, so it shows on the boss's effect bar; no duration, so it lasts the rest of the fight.
			Combatant->ApplyEffect(this, Phase.EffectName, Phase.Modifiers, 1, FGameplayTagContainer(), 0.0f);
		}
	}

	if (Phase.bUnlockPhaseAttacks && !bPhaseAttacksUnlocked)
	{
		bPhaseAttacksUnlocked = true;
		Attacks.Append(PhaseAttacks);
	}

	if (!Phase.Announcement.IsEmpty())
	{
		if (UAdventureComponent* Adventure = UAdventureComponent::Get(this))
		{
			Adventure->ShowBanner(Phase.Announcement, FText::GetEmpty(), 3.0f);
		}
	}

	OnPhaseStarted(PhaseIndex);
}

/* -------------------- Death -------------------- */

void ABossEnemy::HandleCharacterDied(AActor* Killer)
{
	GetWorldTimerManager().ClearTimer(BossBarTimer);

	UAdventureComponent* Adventure = UAdventureComponent::Get(this);

	if (IsValid(Adventure))
	{
		if (Adventure->GetActiveBoss() == this)
		{
			Adventure->SetActiveBoss(nullptr);
		}

		Adventure->ShowBanner(FText::Format(LOCTEXT("Defeated", "{0} defeated"), GetCharacterDisplayName()), DefeatText, 6.0f);
	}

	bBossBarShown = false;

	// Corpse, loot and experience as for any enemy.
	Super::HandleCharacterDied(Killer);

	// Last: the flag can open gates and win the game, which should read after the boss's own banner.
	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->SetFlag(BossId);
		Progress->SaveGame();
	}
}

#undef LOCTEXT_NAMESPACE
