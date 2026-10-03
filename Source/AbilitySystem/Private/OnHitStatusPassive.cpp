#include "OnHitStatusPassive.h"

#include "CombatantComponent.h"
#include "GameFramework/Character.h"

void UOnHitStatusPassive::OnDamageDealt_Implementation(AActor* Target, const FAbilityPayload& Payload, const float DamageApplied)
{
	Super::OnDamageDealt_Implementation(Target, Payload, DamageApplied);

	if (!TriggerAbilityTags.IsEmpty() && !Payload.SourceAbilityTags.HasAny(TriggerAbilityTags))
	{
		return;
	}

	UCombatantComponent* TargetCombatant = IsValid(Target) ? Target->FindComponentByClass<UCombatantComponent>() : nullptr;

	if (!IsValid(TargetCombatant) || FMath::FRand() >= GetRankedFloat(ChanceByRank))
	{
		return;
	}

	for (const FStatusEffectSpec& Spec : Statuses)
	{
		if (!Spec.StatusTag.IsValid() || !OwnerHasAllTags(Spec.RequiredOwnerTags))
		{
			continue;
		}

		FStatusApplication Status;
		Status.StatusTag = Spec.StatusTag;
		Status.Modifiers = Spec.Modifiers;
		Status.GrantedTags = Spec.GrantedTags;
		Status.Rank = FMath::Max(GetAbilityRank(), 1);
		Status.Duration = FMath::Max(GetRankedFloat(Spec.DurationByRank), 0.0f);

		// Same attacker as the ability's own statuses, so a shared status tag replaces the base version.
		TargetCombatant->ApplyStatus(Status, GetOwningCharacter());
	}
}
