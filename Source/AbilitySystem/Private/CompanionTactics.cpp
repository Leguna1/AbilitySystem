#include "CompanionTactics.h"

#include "AbilityComponent.h"
#include "BaseCharacter.h"
#include "CombatantComponent.h"
#include "CompanionAIController.h"
#include "CompanionCharacter.h"
#include "CompanionNeedsComponent.h"
#include "ResourceComponent.h"

/* ============================== Context ============================== */

float FCompanionTacticContext::GetHealthFraction(const AActor* Actor)
{
	const UResourceComponent* Resources = IsValid(Actor) ? Actor->FindComponentByClass<UResourceComponent>() : nullptr;

	if (!IsValid(Resources))
	{
		return 1.0f;
	}

	const float MaxHealth = Resources->GetResourceValue(EResourceType::Health, EResourceValueType::Max);

	return MaxHealth > 0.0f
		? FMath::Clamp(Resources->GetResourceValue(EResourceType::Health, EResourceValueType::Current) / MaxHealth, 0.0f, 1.0f)
		: 1.0f;
}

FGameplayTagContainer FCompanionTacticContext::GetActorTags(const AActor* Actor)
{
	FGameplayTagContainer Tags;

	if (!IsValid(Actor))
	{
		return Tags;
	}

	if (const UCombatantComponent* Combatant = Actor->FindComponentByClass<UCombatantComponent>())
	{
		Tags.AppendTags(Combatant->GetEffectTags());
	}

	// Loose states such as State.Staggered live on the ability component.
	if (const UAbilityComponent* Abilities = Actor->FindComponentByClass<UAbilityComponent>())
	{
		Tags.AppendTags(Abilities->GetOwnedGameplayTags());
	}

	return Tags;
}

/* ============================== Base ============================== */

float UCompanionConsideration::Score(const FCompanionTacticContext& Context) const
{
	return IsMet(Context) ? ScoreIfMet : ScoreIfNotMet;
}

/* ============================== Leader ============================== */

bool UConsideration_LeaderHealthBelow::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Leader) && FCompanionTacticContext::GetHealthFraction(Context.Leader) < Fraction;
}

bool UConsideration_EnemiesNearLeader::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Leader) && IsValid(Context.Brain) &&
		Context.Brain->CountHostilesNear(Context.Leader->GetActorLocation(), Radius) >= Count;
}

bool UConsideration_LeaderHasTags::IsMet(const FCompanionTacticContext& Context) const
{
	return !Tags.IsEmpty() && FCompanionTacticContext::GetActorTags(Context.Leader).HasAny(Tags);
}

bool UConsideration_LeaderUsingAbility::IsMet(const FCompanionTacticContext& Context) const
{
	const UAbilityComponent* Abilities = IsValid(Context.Leader) ? Context.Leader->GetAbilityComponent() : nullptr;
	const UActiveAbility* Active = IsValid(Abilities) ? Abilities->GetActiveAbility() : nullptr;

	if (!IsValid(Active))
	{
		return false;
	}

	return AbilityTags.IsEmpty() || Active->GetAbilityTags().HasAny(AbilityTags);
}

bool UConsideration_LeaderNeedAbove::IsMet(const FCompanionTacticContext& Context) const
{
	const UCompanionNeedsComponent* Needs = IsValid(Context.Companion) ? Context.Companion->GetNeedsComponent() : nullptr;
	return IsValid(Needs) && Needs->GetNeed(Need) >= Threshold;
}

/* ============================== Self ============================== */

bool UConsideration_SelfHealthBelow::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Companion) && FCompanionTacticContext::GetHealthFraction(Context.Companion) < Fraction;
}

bool UConsideration_FightJustStarted::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Brain) && Context.Brain->IsInFight() &&
		Context.Brain->GetWorldTime() - Context.Brain->GetFightStartTime() <= Within;
}

/* ============================== Target ============================== */

bool UConsideration_TargetHasTags::IsMet(const FCompanionTacticContext& Context) const
{
	return !Tags.IsEmpty() && FCompanionTacticContext::GetActorTags(Context.Target).HasAny(Tags);
}

bool UConsideration_TargetHealthBelow::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Target) && FCompanionTacticContext::GetHealthFraction(Context.Target) < Fraction;
}

bool UConsideration_TargetThreatensLeader::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Brain) && IsValid(Context.Target) &&
		Context.Brain->IsThreatening(Context.Target, Context.Leader, Within);
}

bool UConsideration_TargetThreatensSelf::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Brain) && IsValid(Context.Target) &&
		Context.Brain->IsThreatening(Context.Target, Context.Companion, Within);
}

bool UConsideration_TargetIsLeaderTarget::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Brain) && IsValid(Context.Target) && Context.Brain->GetLeaderTarget() == Context.Target;
}

bool UConsideration_EnemiesNearTarget::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Brain) && IsValid(Context.Target) &&
		Context.Brain->CountHostilesNear(Context.Target->GetActorLocation(), Radius) >= Count;
}

bool UConsideration_TargetIsMoving::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Target) && Context.Target->GetVelocity().Size2D() > Speed;
}

bool UConsideration_TargetNearLeader::IsMet(const FCompanionTacticContext& Context) const
{
	return IsValid(Context.Leader) && IsValid(Context.Target) &&
		FVector::Dist(Context.Leader->GetActorLocation(), Context.Target->GetActorLocation()) <= Distance;
}
