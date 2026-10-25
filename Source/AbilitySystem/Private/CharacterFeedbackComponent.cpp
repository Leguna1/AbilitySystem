// CharacterFeedbackComponent.cpp
#include "CharacterFeedbackComponent.h"

#include "CombatantComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "FeedbackTags.h"
#include "GameFramework/Character.h"
#include "GameplayTagsManager.h"

namespace
{
	const TCHAR* StatusPrefix = TEXT("Status.");
}

UCharacterFeedbackComponent::UCharacterFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	Combatant = IsValid(GetOwner()) ? GetOwner()->FindComponentByClass<UCombatantComponent>() : nullptr;

	if (!IsValid(Combatant))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Character Feedback needs a Combatant component on %s; only manual cues will play."),
			*GetName(), *GetNameSafe(GetOwner()));
		return;
	}

	Combatant->OnEffectsChanged.AddDynamic(this, &UCharacterFeedbackComponent::HandleEffectsChanged);
	Combatant->OnPayloadReceived.AddDynamic(this, &UCharacterFeedbackComponent::HandlePayloadReceived);
	Combatant->OnPoiseDamaged.AddDynamic(this, &UCharacterFeedbackComponent::HandlePoiseDamaged);
	Combatant->OnDied.AddDynamic(this, &UCharacterFeedbackComponent::HandleDied);
	Combatant->OnRevived.AddDynamic(this, &UCharacterFeedbackComponent::HandleRevived);

	// Statuses applied before this began (spawned already burning).
	SyncStatusLoops();
}

void UCharacterFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(Combatant))
	{
		Combatant->OnEffectsChanged.RemoveDynamic(this, &UCharacterFeedbackComponent::HandleEffectsChanged);
		Combatant->OnPayloadReceived.RemoveDynamic(this, &UCharacterFeedbackComponent::HandlePayloadReceived);
		Combatant->OnPoiseDamaged.RemoveDynamic(this, &UCharacterFeedbackComponent::HandlePoiseDamaged);
		Combatant->OnDied.RemoveDynamic(this, &UCharacterFeedbackComponent::HandleDied);
		Combatant->OnRevived.RemoveDynamic(this, &UCharacterFeedbackComponent::HandleRevived);
	}

	StatusLoops.Reset();
	Super::EndPlay(EndPlayReason);
}

USceneComponent* UCharacterFeedbackComponent::GetAttachTarget() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (IsValid(Character->GetMesh()))
		{
			return Character->GetMesh();
		}
	}

	return IsValid(GetOwner()) ? GetOwner()->GetRootComponent() : nullptr;
}

FVector UCharacterFeedbackComponent::GetBodyLocation() const
{
	const USceneComponent* Target = GetAttachTarget();
	return IsValid(Target) ? Target->Bounds.Origin : (IsValid(GetOwner()) ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
}

bool UCharacterFeedbackComponent::PlayCueOnBody(const FGameplayTag Cue)
{
	const AActor* OwnerActor = GetOwner();
	return PlayCueAt(Cue, GetBodyLocation(), IsValid(OwnerActor) ? OwnerActor->GetActorRotation() : FRotator::ZeroRotator);
}

/* -------------------- Statuses -------------------- */

void UCharacterFeedbackComponent::HandleEffectsChanged()
{
	SyncStatusLoops();
}

FGameplayTag UCharacterFeedbackComponent::GetCueForStatus(const FGameplayTag& StatusTag)
{
	// "Status.Burning" -> "Cue.Status.Burning", only if someone added that tag.
	const FName CueName(*(TEXT("Cue.") + StatusTag.ToString()));
	const FGameplayTag SpecificCue = UGameplayTagsManager::Get().RequestGameplayTag(CueName, false);

	return SpecificCue.IsValid() ? SpecificCue : FeedbackTags::Cue_Status;
}

void UCharacterFeedbackComponent::SyncStatusLoops()
{
	TArray<FGameplayTag> CurrentStatuses;

	if (bShowStatuses && IsValid(Combatant))
	{
		FGameplayTagContainer EffectTags = Combatant->GetEffectTags();

		for (const FGameplayTag& Tag : EffectTags)
		{
			if (Tag.ToString().StartsWith(StatusPrefix))
			{
				CurrentStatuses.Add(Tag);
			}
		}
	}

	// Ended: stop their loops (Stop Effect plays).
	for (auto It = StatusLoops.CreateIterator(); It; ++It)
	{
		if (!CurrentStatuses.Contains(It.Key()))
		{
			StopLoop(It.Value());
			It.RemoveCurrent();
		}
	}

	// New: start theirs (Start Effect plays). Several statuses sharing Cue.Status share one loop.
	USceneComponent* AttachTarget = GetAttachTarget();

	for (const FGameplayTag& Status : CurrentStatuses)
	{
		if (!StatusLoops.Contains(Status))
		{
			const FGameplayTag Cue = GetCueForStatus(Status);
			StatusLoops.Add(Status, Cue);
			StartLoop(Cue, AttachTarget);
		}
	}
}

/* -------------------- Reactions -------------------- */

void UCharacterFeedbackComponent::HandlePayloadReceived(const FAbilityPayload& Payload, const float DamageApplied)
{
	const UWorld* World = GetWorld();

	if (DamageApplied <= 0.0f ||
		!IsValid(World) ||
		(IsValid(Combatant) && Combatant->IsDead()) ||
		PoiseBreakFrame == GFrameCounter)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();

	if (Now - LastHurtCueTime < HurtCueInterval)
	{
		return;
	}

	LastHurtCueTime = Now;
	PlayCueOnBody(FeedbackTags::Cue_Character_Hurt);
}

void UCharacterFeedbackComponent::HandlePoiseDamaged(const float Poise, const float MaxPoise, const bool bBroken)
{
	if (!bBroken)
	{
		return;
	}

	// Broadcast before the payload-received event of the same hit: that one skips Hurt.
	PoiseBreakFrame = GFrameCounter;
	PlayCueOnBody(FeedbackTags::Cue_Character_PoiseBreak);
}

void UCharacterFeedbackComponent::HandleDied(AActor* Killer)
{
	PlayCueOnBody(FeedbackTags::Cue_Character_Death);
}

void UCharacterFeedbackComponent::HandleRevived()
{
	PlayCueOnBody(FeedbackTags::Cue_Character_Revive);
	SyncStatusLoops();
}
