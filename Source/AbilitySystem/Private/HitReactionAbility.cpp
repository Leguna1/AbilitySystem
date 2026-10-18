#include "HitReactionAbility.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"

#include "GameFramework/Character.h"

UHitReactionAbility::UHitReactionAbility()
{
	// A stagger lasts as long as it lasts, whatever buffs or debuffs the character has.
	bAffectedByAttackSpeed = false;
}
void UHitReactionAbility::ActivateAbility_Implementation()
{
	Super::ActivateAbility_Implementation();

	UAbilityComponent* Component = GetAbilityComponent();
	HitReaction = IsValid(Component) ? Component->GetPendingHitReaction() : FHitReactionResult();

	if (IsValid(Component))
	{
		Component->AddLooseOwnerTag(AbilitySystemTags::State_Staggered);
		bStaggerTagApplied = true;
	}

	UAnimMontage* Montage = SelectReactionMontage(HitReaction);
	
	// Knockback from a poise break: pushed away from the attacker, with optional lift.
	if (HitReaction.KnockbackSpeed > 0.0f || HitReaction.KnockbackLift > 0.0f)
	{
		if (ACharacter* Victim = GetOwningCharacter())
		{
			const FVector LaunchVelocity =
				HitReaction.HitDirection * HitReaction.KnockbackSpeed +
				FVector::UpVector * HitReaction.KnockbackLift;

			Victim->LaunchCharacter(LaunchVelocity, true, HitReaction.KnockbackLift > 0.0f);
		}
	}

	// No animation: the interrupt still happened, so just end straight away.
	if (!IsValid(Montage) || !PlayAbilityMontage(Montage, ReactionPlayRate))
	{
		RequestEndAbility();
	}
}

void UHitReactionAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	if (bStaggerTagApplied)
	{
		if (UAbilityComponent* Component = GetAbilityComponent())
		{
			Component->RemoveLooseOwnerTag(AbilitySystemTags::State_Staggered);
		}

		bStaggerTagApplied = false;
	}

	Super::OnAbilityEnded_Implementation(EndReason);
}

UAnimMontage* UHitReactionAbility::SelectReactionMontage_Implementation(const FHitReactionResult& Reaction) const
{
	return StaggerMontages.Get(Reaction.Direction);
}
