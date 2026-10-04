#include "OffensiveAbilityBase.h"

#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "TargetingComponent.h"
#include "WeaponManagerComponent.h"
#include "AbilitySystemTags.h"

void UOffensiveAbilityBase::ActivateAbility_Implementation()
{
	ConfigureTargetFacingWarp();

	Super::ActivateAbility_Implementation();

	if (GetAbilityStatus() == EAbilityStatus::Active)
	{
		OnAttackStarted();
	}
}

void UOffensiveAbilityBase::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	Super::OnAbilityEnded_Implementation(EndReason);

	if (bRemoveTargetFacingWarpWhenFinished &&
		!TargetFacingWarpName.IsNone() &&
		IsValid(GetMotionWarpingComponent()))
	{
		GetMotionWarpingComponent()->RemoveWarpTarget(TargetFacingWarpName);
	}

	OnAttackFinished(EndReason);
}

bool UOffensiveAbilityBase::ConfigureTargetFacingWarp()
{
	if (!bUseTargetFacingWarp ||
		TargetFacingWarpName.IsNone() ||
		!IsValid(GetMotionWarpingComponent()) ||
		!IsValid(GetTargetingComponent()) ||
		!GetTargetingComponent()->HasTarget())
	{
		return false;
	}

	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character))
	{
		return false;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	const FVector TargetLocation = GetTargetingComponent()->GetCurrentTargetAimLocation();

	FVector FlatDirection = TargetLocation - CharacterLocation;
	FlatDirection.Z = 0.0f;

	if (FlatDirection.IsNearlyZero())
	{
		return false;
	}

	
	GetMotionWarpingComponent()->AddOrUpdateWarpTargetFromLocationAndRotation(
		TargetFacingWarpName,
		CharacterLocation,
		FlatDirection.Rotation()
	);

	return true;
}

void UOffensiveAbilityBase::OnAttackStarted_Implementation()
{
}

void UOffensiveAbilityBase::OnAttackFinished_Implementation(EAbilityEndReason EndReason)
{
}
UWeaponManagerComponent* UOffensiveAbilityBase::GetWeaponManager() const
{
	const ACharacter* Character = GetOwningCharacter();

	return IsValid(Character)
		? Character->FindComponentByClass<UWeaponManagerComponent>()
		: nullptr;
}

AWeaponBase* UOffensiveAbilityBase::GetEquippedWeapon() const
{
	const UWeaponManagerComponent* WeaponManager = GetWeaponManager();

	return IsValid(WeaponManager) ? WeaponManager->GetEquippedWeapon() : nullptr;
}
void UOffensiveAbilityBase::ResolveOnHitStatuses(TArray<FStatusApplication>& OutStatuses) const
{
	ResolveStatusSpecs(OnHitStatuses, OutStatuses);
}

void UOffensiveAbilityBase::ResolveStatusSpecs(const TArray<FStatusEffectSpec>& Specs, TArray<FStatusApplication>& OutStatuses) const
{
	OutStatuses.Reset();

	for (const FStatusEffectSpec& Spec : Specs)
	{
		if (!Spec.StatusTag.IsValid() || !OwnerHasAllTags(Spec.RequiredOwnerTags))
		{
			continue;
		}

		const float Duration = GetRankedFloat(Spec.DurationByRank);

		if (Duration <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: status %s has no duration and was skipped."),
				*GetName(), *Spec.StatusTag.ToString());
			continue;
		}

		FStatusApplication& Status = OutStatuses.AddDefaulted_GetRef();
		Status.StatusTag = Spec.StatusTag;
		Status.Modifiers = Spec.Modifiers;
		Status.GrantedTags = Spec.GrantedTags;
		Status.Rank = FMath::Max(GetAbilityRank(), 1);
		Status.Duration = Duration;
	}
}
FHitImpact UOffensiveAbilityBase::ResolveHitImpact() const
{
	FHitImpact Impact = HitImpact;
	Impact.PoiseDamage = FMath::Max(GetModifiedFloat(AbilitySystemTags::Stat_PoiseDamage, HitImpact.PoiseDamage), 0.0f);
	return Impact;
}