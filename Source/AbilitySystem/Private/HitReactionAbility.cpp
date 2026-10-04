#include "HitReactionAbility.h"

#include "AbilityComponent.h"
#include "AbilitySystemTags.h"

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