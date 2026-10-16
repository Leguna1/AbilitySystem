#include "ExperienceComponent.h"

#include "AttributeComponent.h"
#include "BaseEnemy.h"
#include "CombatantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ResourceComponent.h"
#include "SkillTreeComponent.h"
#include "Sound/SoundBase.h"

UExperienceComponent::UExperienceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExperienceComponent::BeginPlay()
{
	Super::BeginPlay();

	Level = FMath::Clamp(StartingLevel, 1, FMath::Max(MaxLevel, 1));
	Experience = 0;

	CombatantComponent = GetOwner()->FindComponentByClass<UCombatantComponent>();
	SkillTreeComponent = GetOwner()->FindComponentByClass<USkillTreeComponent>();
	AttributeComponent = GetOwner()->FindComponentByClass<UAttributeComponent>();
	ResourceComponent = GetOwner()->FindComponentByClass<UResourceComponent>();

	// This component owns the level: attributes start at it.
	if (IsValid(AttributeComponent))
	{
		AttributeComponent->SetCharacterLevel(Level);
	}

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnKilled.AddDynamic(this, &UExperienceComponent::HandleKilled);
	}

	BroadcastExperienceChanged();
}

void UExperienceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnKilled.RemoveDynamic(this, &UExperienceComponent::HandleKilled);
	}

	Super::EndPlay(EndPlayReason);
}

void UExperienceComponent::AddExperience(const int32 Amount)
{
	if (Amount <= 0 || IsMaxLevel())
	{
		return;
	}

	Experience += Amount;

	const int32 LevelBefore = Level;

	// One kill can be worth several levels; each one is announced and rewarded.
	while (!IsMaxLevel() && Experience >= GetExperienceToNextLevel())
	{
		Experience -= GetExperienceToNextLevel();
		++Level;

		if (IsValid(SkillTreeComponent) && SkillPointsPerLevel > 0)
		{
			SkillTreeComponent->AddSkillPoints(SkillPointsPerLevel);
		}

		if (IsValid(AttributeComponent))
		{
			AttributeComponent->SetCharacterLevel(Level);
			AttributeComponent->AddAttributePoints(AttributePointsPerLevel);
		}

		OnLevelUp.Broadcast(Level);
	}

	if (IsMaxLevel())
	{
		Experience = 0;
	}

	// One effect and sound for the whole gain, not one per level stacked on top of each other.
	if (Level > LevelBefore)
	{
		// After the level is applied, so the refill fills the new, higher max.
		if (bRefillResourcesOnLevelUp && IsValid(ResourceComponent))
		{
			ResourceComponent->RefillResources();
		}

		PlayLevelUpFeedback();
	}

	BroadcastExperienceChanged();
}

void UExperienceComponent::PlayLevelUpFeedback() const
{
	if (IsValid(LevelUpEffect))
	{
		const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
		USceneComponent* AttachTo = IsValid(OwnerCharacter) && IsValid(OwnerCharacter->GetMesh())
			? static_cast<USceneComponent*>(OwnerCharacter->GetMesh())
			: GetOwner()->GetRootComponent();

		UNiagaraFunctionLibrary::SpawnSystemAttached(
			LevelUpEffect, AttachTo, LevelUpEffectSocket,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true);
	}

	if (IsValid(LevelUpSound))
	{
		UGameplayStatics::PlaySound2D(this, LevelUpSound);
	}
}

int32 UExperienceComponent::GetExperienceToNextLevel() const
{
	return IsMaxLevel() ? 0 : GetExperienceRequiredForLevel(Level);
}

float UExperienceComponent::GetLevelProgress() const
{
	const int32 Required = GetExperienceToNextLevel();
	return Required > 0 ? FMath::Clamp(static_cast<float>(Experience) / static_cast<float>(Required), 0.0f, 1.0f) : 1.0f;
}

int32 UExperienceComponent::GetExperienceRequiredForLevel(const int32 ForLevel) const
{
	const float Required = static_cast<float>(BaseExperience) * FMath::Pow(static_cast<float>(FMath::Max(ForLevel, 1)), ExperienceGrowth);
	return FMath::Max(FMath::RoundToInt(Required), 1);
}

int32 UExperienceComponent::GetExperienceForKill_Implementation(AActor* Victim) const
{
	const ABaseEnemy* Enemy = Cast<ABaseEnemy>(Victim);
	return IsValid(Enemy) ? Enemy->GetExperienceReward() : 0;
}

void UExperienceComponent::HandleKilled(AActor* Victim)
{
	AddExperience(GetExperienceForKill(Victim));
}

void UExperienceComponent::BroadcastExperienceChanged()
{
	OnExperienceChanged.Broadcast(Experience, GetExperienceToNextLevel(), Level);
}
