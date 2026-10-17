#include "CompanionComponent.h"

#include "Ability.h"
#include "AbilityComponent.h"
#include "ActionBarComponent.h"
#include "AttributeComponent.h"
#include "BaseCharacter.h"
#include "CombatantComponent.h"
#include "CompanionCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"
#include "ExperienceComponent.h"
#include "ResourceComponent.h"
#include "TargetingComponent.h"

UCompanionComponent::UCompanionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCompanionComponent::BeginPlay()
{
	Super::BeginPlay();

	Stance = DefaultStance;

	if (UExperienceComponent* Experience = GetOwner()->FindComponentByClass<UExperienceComponent>())
	{
		Experience->OnLevelUp.AddDynamic(this, &UCompanionComponent::HandleLeaderLevelUp);
	}

	if (UAbilityComponent* Abilities = GetOwner()->FindComponentByClass<UAbilityComponent>())
	{
		Abilities->AbilityRankChangedEvent.AddDynamic(this, &UCompanionComponent::HandleLeaderAbilityRankChanged);
	}
}

void UCompanionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UExperienceComponent* Experience = GetOwner()->FindComponentByClass<UExperienceComponent>())
	{
		Experience->OnLevelUp.RemoveDynamic(this, &UCompanionComponent::HandleLeaderLevelUp);
	}

	if (UAbilityComponent* Abilities = GetOwner()->FindComponentByClass<UAbilityComponent>())
	{
		Abilities->AbilityRankChangedEvent.RemoveDynamic(this, &UCompanionComponent::HandleLeaderAbilityRankChanged);
	}

	// The pet leaves with its leader.
	DismissCompanion();

	Super::EndPlay(EndPlayReason);
}

/* ============================== Companion ============================== */

ACompanionCharacter* UCompanionComponent::GetCompanion() const
{
	ACompanionCharacter* Current = Companion.Get();
	return IsValid(Current) && !Current->IsDead() ? Current : nullptr;
}

bool UCompanionComponent::HasCompanion() const
{
	return IsValid(GetCompanion());
}

ACompanionAIController* UCompanionComponent::GetCompanionController() const
{
	const ACompanionCharacter* Current = GetCompanion();
	return IsValid(Current) ? Cast<ACompanionAIController>(Current->GetController()) : nullptr;
}

ACompanionCharacter* UCompanionComponent::SpawnCompanion(const TSubclassOf<ACompanionCharacter> CompanionClass, const FVector Location,
	const int32 LevelOffset, const TArray<FCompanionAbilityGrant>& AbilityGrants, const FGameplayTag SourceAbilityId)
{
	UWorld* World = GetWorld();
	ABaseCharacter* Leader = Cast<ABaseCharacter>(GetOwner());

	if (!CompanionClass || !IsValid(World) || !IsValid(Leader))
	{
		return nullptr;
	}

	DismissCompanion();

	// A new companion replaces the old body too.
	ClearCorpse();
	LastCompanionClass = CompanionClass;

	// Location is the ground; the actor goes at its capsule center.
	const float HalfHeight = CompanionClass.GetDefaultObject()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	// A couple of centimetres of air, so it never starts inside the ground.
	const FTransform SpawnTransform(Leader->GetActorRotation(), Location + FVector(0.0f, 0.0f, HalfHeight + 2.0f));

	// Deferred: leader, team and level are set before it begins play and gets possessed.
	ACompanionCharacter* NewCompanion = World->SpawnActorDeferred<ACompanionCharacter>(
		CompanionClass, SpawnTransform, Leader, Leader, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!IsValid(NewCompanion))
	{
		return nullptr;
	}

	NewCompanion->SetLeader(Leader);

	CompanionLevelOffset = LevelOffset;
	RespawnAbilityId = SourceAbilityId;
	ActiveGrants = AbilityGrants;

	if (UAttributeComponent* Attributes = NewCompanion->FindComponentByClass<UAttributeComponent>())
	{
		Attributes->SetCharacterLevel(GetLeaderLevel() + CompanionLevelOffset);
	}

	NewCompanion->FinishSpawning(SpawnTransform);

	if (UCombatantComponent* Combatant = NewCompanion->GetCombatantComponent())
	{
		Combatant->OnDied.AddDynamic(this, &UCompanionComponent::HandleCompanionDied);
	}

	Companion = NewCompanion;

	UE_LOG(LogTemp, Log, TEXT("[Companion] Summoned %s, level %d, %d grant(s), companion bar slots: %d."),
		*GetNameSafe(NewCompanion), GetLeaderLevel() + CompanionLevelOffset, ActiveGrants.Num(),
		IsValid(NewCompanion->GetCompanionActionBar()) ? NewCompanion->GetCompanionActionBar()->GetNumSlots() : -1);

	ApplyAbilityGrants();
	ApplyChoicesToCompanion();

	OnCompanionChanged.Broadcast(NewCompanion);
	return NewCompanion;
}

FVector UCompanionComponent::FindPlacementNearLeader(const ACharacter* Leader, const FVector PreferredDirection, const float Distance)
{
	if (!IsValid(Leader) || !IsValid(Leader->GetWorld()))
	{
		return FVector::ZeroVector;
	}

	UWorld* World = Leader->GetWorld();
	const float LeaderHalfHeight = Leader->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector LeaderCenter = Leader->GetActorLocation();
	const FVector LeaderFeet = LeaderCenter - FVector(0.0f, 0.0f, LeaderHalfHeight);

	// Ground may be a little above (a step up) or below (a step down) the leader's feet, never a floor away.
	constexpr float MaxStepUp = 60.0f;
	constexpr float MaxDrop = 120.0f;
	constexpr float MinWalkableNormalZ = 0.7f;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(CompanionPlacement), false, Leader);
	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	FVector Forward = PreferredDirection.GetSafeNormal2D();

	if (Forward.IsNearlyZero())
	{
		Forward = Leader->GetActorRightVector();
	}

	const UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	// The preferred side first, then swinging around the leader.
	static const float Angles[] = { 0.0f, 180.0f, 90.0f, -90.0f, 45.0f, -45.0f, 135.0f, -135.0f };

	for (const float Angle : Angles)
	{
		const FVector Direction = Forward.RotateAngleAxis(Angle, FVector::UpVector);
		const FVector CandidateFeet = LeaderFeet + Direction * Distance;
		const FVector CandidateCenter = CandidateFeet + FVector(0.0f, 0.0f, LeaderHalfHeight);

		FHitResult Hit;

		// A wall, a rock or a railing between the leader and the spot: not there.
		if (World->LineTraceSingleByChannel(Hit, LeaderCenter, CandidateCenter, ECC_Visibility, QueryParams, ResponseParams))
		{
			continue;
		}

		// Walkable ground close to the leader's height. Past a platform's edge there is none within MaxDrop.
		const FVector DownStart = CandidateFeet + FVector(0.0f, 0.0f, MaxStepUp);
		const FVector DownEnd = CandidateFeet - FVector(0.0f, 0.0f, MaxDrop);

		if (!World->LineTraceSingleByChannel(Hit, DownStart, DownEnd, ECC_Visibility, QueryParams, ResponseParams) ||
			Hit.ImpactNormal.Z < MinWalkableNormalZ)
		{
			continue;
		}

		FVector Ground = Hit.ImpactPoint;

		// On the nav mesh if it's right there, so the companion can walk from the start. Tight vertical extent:
		// the floor under a platform must never win.
		if (IsValid(NavSystem))
		{
			FNavLocation OnNavMesh;

			if (NavSystem->ProjectPointToNavigation(Ground, OnNavMesh, FVector(50.0f, 50.0f, 50.0f)))
			{
				Ground = OnNavMesh.Location;
			}
		}

		return Ground;
	}

	// Nowhere around (a narrow ledge): at the leader's feet; spawning and teleporting push it out of the capsule.
	return LeaderFeet;
}

ACompanionCharacter* UCompanionComponent::GetCompanionCorpse() const
{
	ACompanionCharacter* Body = Corpse.Get();
	return IsValid(Body) && !Body->IsActorBeingDestroyed() ? Body : nullptr;
}

bool UCompanionComponent::CanReviveCompanion(const float MaxDistance) const
{
	const ACompanionCharacter* Body = GetCompanionCorpse();

	if (!IsValid(Body) || !LastCompanionClass || HasCompanion())
	{
		return false;
	}

	return MaxDistance <= 0.0f || FVector::Dist(Body->GetActorLocation(), GetOwner()->GetActorLocation()) <= MaxDistance;
}

ACompanionCharacter* UCompanionComponent::ReviveCompanion(const float HealthFraction)
{
	if (!CanReviveCompanion())
	{
		return nullptr;
	}

	ACompanionCharacter* Body = GetCompanionCorpse();
	const FVector Ground = Body->GetActorLocation() - FVector(0.0f, 0.0f, Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// A fresh companion where the body lies: no death state to undo, same class, level and skills.
	// Copies first: SpawnCompanion overwrites the remembered grants and id.
	const TArray<FCompanionAbilityGrant> Grants = ActiveGrants;
	const FGameplayTag SourceAbilityId = RespawnAbilityId;

	ACompanionCharacter* Revived = SpawnCompanion(LastCompanionClass, Ground, CompanionLevelOffset, Grants, SourceAbilityId);

	if (!IsValid(Revived))
	{
		return nullptr;
	}

	if (UResourceComponent* Resources = Revived->GetResourceComponent())
	{
		const float MaxHealth = Resources->GetResourceValue(EResourceType::Health, EResourceValueType::Max);
		const float CurrentHealth = Resources->GetResourceValue(EResourceType::Health, EResourceValueType::Current);
		Resources->ModifyResource(EResourceType::Health, EResourceValueType::Current, MaxHealth * FMath::Clamp(HealthFraction, 0.01f, 1.0f) - CurrentHealth);
	}

	// Back without waiting: the respawn cooldown is over.
	if (UAbilityComponent* Abilities = SourceAbilityId.IsValid() ? GetOwner()->FindComponentByClass<UAbilityComponent>() : nullptr)
	{
		Abilities->ReduceCooldowns(FGameplayTagContainer(SourceAbilityId), 0.0f, 1.0f, true);
	}

	return Revived;
}

void UCompanionComponent::ClearCorpse()
{
	if (ACompanionCharacter* Body = Corpse.Get())
	{
		Body->Destroy();
	}

	Corpse.Reset();
}

void UCompanionComponent::DismissCompanion()
{
	ACompanionCharacter* Current = Companion.Get();

	if (!IsValid(Current))
	{
		return;
	}

	ReleaseCompanion();
	Current->Destroy();
	OnCompanionChanged.Broadcast(nullptr);
}

void UCompanionComponent::ReleaseCompanion()
{
	if (ACompanionCharacter* Current = Companion.Get())
	{
		if (UCombatantComponent* Combatant = Current->GetCombatantComponent())
		{
			Combatant->OnDied.RemoveDynamic(this, &UCompanionComponent::HandleCompanionDied);
		}
	}

	Companion.Reset();
}

void UCompanionComponent::HandleCompanionDied(AActor* Killer)
{
	// The body stays for its Corpse Lifetime (a revive can bring it back); as a companion it's gone now.
	Corpse = Companion;
	ReleaseCompanion();

	// The summon ability's own cooldown is the respawn wait, reduced by any Stat.Cooldown modifiers on it.
	if (UAbilityComponent* Abilities = RespawnAbilityId.IsValid() ? GetOwner()->FindComponentByClass<UAbilityComponent>() : nullptr)
	{
		Abilities->StartCooldown(RespawnAbilityId, Abilities->GetAbilityCooldownDuration(RespawnAbilityId));
	}

	OnCompanionChanged.Broadcast(nullptr);
}

/* ============================== Level and grants ============================== */

void UCompanionComponent::HandleLeaderLevelUp(const int32 NewLevel)
{
	const ACompanionCharacter* Current = GetCompanion();

	if (UAttributeComponent* Attributes = IsValid(Current) ? Current->GetAttributeComponent() : nullptr)
	{
		Attributes->SetCharacterLevel(NewLevel + CompanionLevelOffset);
	}
}

void UCompanionComponent::HandleLeaderAbilityRankChanged(FGameplayTag AbilityId, int32 OldRank, int32 NewRank)
{
	// Learning a summon path or upgrade changes the companion right away, not at the next summon.
	ApplyAbilityGrants();
}

void UCompanionComponent::ApplyAbilityGrants()
{
	const ACompanionCharacter* Current = GetCompanion();
	UAbilityComponent* CompanionAbilities = IsValid(Current) ? Current->GetAbilityComponent() : nullptr;
	const UAbilityComponent* LeaderAbilities = GetOwner()->FindComponentByClass<UAbilityComponent>();

	if (!IsValid(CompanionAbilities) || !IsValid(LeaderAbilities))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Companion] Grants skipped: %s."),
			!IsValid(Current) ? TEXT("no living companion") : TEXT("missing an Ability Component"));
		return;
	}

	if (ActiveGrants.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Companion] No Ability Grants: the summon ability's Ability Grants list is empty."));
	}

	for (const FCompanionAbilityGrant& Grant : ActiveGrants)
	{
		const UAbility* GrantDefaults = Grant.CompanionAbility ? Grant.CompanionAbility.GetDefaultObject() : nullptr;

		if (!IsValid(GrantDefaults))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Companion] A grant row has no Companion Ability."));
			continue;
		}

		const int32 RequiredRank = Grant.RequiredAbility ? LeaderAbilities->GetAbilityRank(Grant.RequiredAbility) : 1;

		// Granted with this component as the source, so a revoke never touches the companion's own kit.
		if (RequiredRank <= 0)
		{
			// Unlearned: back to rank 0 first, so it can't be used even if something else also grants it,
			// then drop this grant (the ability leaves the companion and its bar).
			CompanionAbilities->SetLearnedRank(Grant.CompanionAbility, 0);
			CompanionAbilities->RemoveAbility(GrantDefaults->GetAbilityId(), this);

			UE_LOG(LogTemp, Log, TEXT("[Companion] %s: not learned -> removed (companion rank now %d%s)."),
				*GetNameSafe(Grant.CompanionAbility), CompanionAbilities->GetAbilityRank(Grant.CompanionAbility),
				CompanionAbilities->HasAbility(GrantDefaults->GetAbilityId())
					? TEXT(", STILL granted: it's also in the companion's own Starting Ability Classes")
					: TEXT(""));
			continue;
		}

		CompanionAbilities->GrantAbility(Grant.CompanionAbility, this);

		if (Grant.RequiredAbility && Grant.bRankFollowsRequiredAbility)
		{
			// Rank = starting rank + learned rank, so learned = what's left to reach the leader's rank.
			CompanionAbilities->SetLearnedRank(Grant.CompanionAbility, RequiredRank - GrantDefaults->GetStartingRank());
		}
		else if (CompanionAbilities->GetAbilityRank(Grant.CompanionAbility) <= 0)
		{
			// Starting rank 0 and no rank source: rank 1 so it can be used at all.
			CompanionAbilities->SetLearnedRank(Grant.CompanionAbility, 1 - GrantDefaults->GetStartingRank());
		}

		UE_LOG(LogTemp, Log, TEXT("[Companion] %s: leader rank %d -> companion has it at rank %d (granted: %s)."),
			*GetNameSafe(Grant.CompanionAbility), RequiredRank,
			CompanionAbilities->GetAbilityRank(Grant.CompanionAbility),
			CompanionAbilities->HasAbility(GrantDefaults->GetAbilityId()) ? TEXT("yes") : TEXT("NO, check its Ability Id"));
	}
}

int32 UCompanionComponent::GetLeaderLevel() const
{
	if (const UExperienceComponent* Experience = GetOwner()->FindComponentByClass<UExperienceComponent>())
	{
		return Experience->GetLevel();
	}

	const UAttributeComponent* Attributes = GetOwner()->FindComponentByClass<UAttributeComponent>();
	return IsValid(Attributes) ? Attributes->GetCharacterLevel() : 1;
}

/* ============================== Commands ============================== */

AActor* UCompanionComponent::GetLeaderTarget() const
{
	const UTargetingComponent* Targeting = GetOwner()->FindComponentByClass<UTargetingComponent>();
	return IsValid(Targeting) ? Targeting->GetCurrentTarget() : nullptr;
}

bool UCompanionComponent::ActivateCompanionSlot(const int32 SlotIndex)
{
	const ACompanionCharacter* Current = GetCompanion();
	const UActionBarComponent* Bar = IsValid(Current) ? Current->GetCompanionActionBar() : nullptr;
	const UAbility* Defaults = IsValid(Bar) ? Bar->GetSlotAbility(SlotIndex).GetDefaultObject() : nullptr;

	return IsValid(Defaults) && CommandAbility(Defaults->GetAbilityId());
}

bool UCompanionComponent::CommandAbility(const FGameplayTag AbilityId)
{
	ACompanionAIController* Brain = GetCompanionController();
	return IsValid(Brain) && Brain->CommandAbility(AbilityId, GetLeaderTarget());
}

bool UCompanionComponent::CommandAttack(AActor* Target)
{
	ACompanionAIController* Brain = GetCompanionController();
	return IsValid(Brain) && Brain->CommandAttack(Target);
}

void UCompanionComponent::Recall()
{
	if (ACompanionAIController* Brain = GetCompanionController())
	{
		Brain->Recall();
	}
}

/* ============================== Stance and auto-cast ============================== */

void UCompanionComponent::SetStance(const ECompanionStance NewStance)
{
	if (Stance == NewStance)
	{
		return;
	}

	Stance = NewStance;
	ApplyChoicesToCompanion();
	OnStanceChanged.Broadcast(Stance);
}

bool UCompanionComponent::IsAutoCast(const FGameplayTag AbilityId) const
{
	if (const ACompanionAIController* Brain = GetCompanionController())
	{
		return Brain->IsAutoCast(AbilityId);
	}

	const bool* Choice = AutoCastChoices.Find(AbilityId);
	return Choice && *Choice;
}

void UCompanionComponent::SetAutoCast(const FGameplayTag AbilityId, const bool bEnabled)
{
	if (!AbilityId.IsValid())
	{
		return;
	}

	AutoCastChoices.Add(AbilityId, bEnabled);
	ApplyChoicesToCompanion();
	OnAutoCastChanged.Broadcast(AbilityId, bEnabled);
}

void UCompanionComponent::ToggleAutoCast(const FGameplayTag AbilityId)
{
	SetAutoCast(AbilityId, !IsAutoCast(AbilityId));
}

void UCompanionComponent::ApplyChoicesToCompanion() const
{
	ACompanionAIController* Brain = GetCompanionController();

	if (!IsValid(Brain))
	{
		return;
	}

	Brain->SetStance(Stance);

	for (const TPair<FGameplayTag, bool>& Choice : AutoCastChoices)
	{
		Brain->SetAutoCast(Choice.Key, Choice.Value);
	}
}
