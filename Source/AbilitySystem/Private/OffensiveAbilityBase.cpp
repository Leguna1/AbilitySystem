#include "OffensiveAbilityBase.h"

#include "GameFramework/Character.h"
#include "MotionWarpingComponent.h"
#include "WeaponManagerComponent.h"
#include "AbilitySystemTags.h"
#include "AttackFragment.h"
#include "ImpactGroupSubsystem.h"

void UOffensiveAbilityBase::ActivateAbility_Implementation()
{
	ConfigureTargetFacingWarp();

	// Before the montage starts, so the very first hit already belongs to this use.
	OpenImpactGroup();

	Super::ActivateAbility_Implementation();

	if (GetAbilityStatus() == EAbilityStatus::Active)
	{
		OnAttackStarted();
	}
	else
	{
		SealImpactGroup();
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

	// Last: the finish callbacks may still land hits. Arrows in flight keep the group alive.
	SealImpactGroup();
}

void UOffensiveAbilityBase::OpenImpactGroup()
{
	SealImpactGroup();

	if (UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this))
	{
		ImpactGroup = ImpactGroups->OpenGroup(ImpactFeedback);
	}
}

void UOffensiveAbilityBase::SealImpactGroup()
{
	if (!ImpactGroup.IsValid())
	{
		return;
	}

	if (UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this))
	{
		ImpactGroups->SealGroup(ImpactGroup);
	}

	ImpactGroup = FImpactGroupHandle();
}

TArray<FAbilityDisplayStat> UOffensiveAbilityBase::GetDisplayStats_Implementation(const int32 Rank) const
{
	TArray<FAbilityDisplayStat> Stats = Super::GetDisplayStats_Implementation(Rank);

	// Damage type first: it reads as the ability's kind ("Physical", "Magic").
	Stats.Insert(FAbilityDisplayStat(
		NSLOCTEXT("OffensiveAbility", "DamageTypeLabel", "Damage"),
		StaticEnum<EDamageType>()->GetDisplayNameTextByValue(static_cast<int64>(DamageType))), 0);

	if (HitImpact.Reaction != EHitReactionType::None && HitImpact.PoiseDamage > 0.0f)
	{
		Stats.Emplace(NSLOCTEXT("OffensiveAbility", "PoiseLabel", "Poise"), FormatDisplayNumber(HitImpact.PoiseDamage));
	}

	return Stats;
}

bool UOffensiveAbilityBase::ConfigureTargetFacingWarp()
{
	if (!bUseTargetFacingWarp ||
	TargetFacingWarpName.IsNone() ||
	!IsValid(GetMotionWarpingComponent()) ||
	!HasAbilityTarget())
	{
		return false;
	}

	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character))
	{
		return false;
	}

	const FVector CharacterLocation = Character->GetActorLocation();
	const FVector TargetLocation = GetAbilityTargetAimLocation();

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
		Status.Display = Spec.Display;
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
	Impact.KnockbackSpeed = FMath::Max(GetModifiedFloat(AbilitySystemTags::Stat_KnockbackSpeed, HitImpact.KnockbackSpeed), 0.0f);
	Impact.KnockbackLift = FMath::Max(GetModifiedFloat(AbilitySystemTags::Stat_KnockbackLift, HitImpact.KnockbackLift), 0.0f);
	
	return Impact;
}
#if WITH_EDITOR
#include "AbilityValidation.h"

EDataValidationResult UOffensiveAbilityBase::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (HitImpact.Reaction != EHitReactionType::None &&
		HitImpact.PoiseDamage <= 0.0f &&
		!HitImpact.bForceReaction)
	{
		Context.AddWarning(NSLOCTEXT("OffensiveAbility", "NoPoise",
			"Hit Impact has a reaction but 0 Poise Damage, so hits can only ever flinch. Set Poise Damage, or Reaction to None."));
	}

	return Result;
}
#endif
FHitSpec UOffensiveAbilityBase::BuildHitSpec() const
{
	FHitSpec Hit;
	Hit.SourceAbilityTags = GetAbilityTags();
	Hit.Impact = ResolveHitImpact();
	Hit.DamageType = DamageType;

	ForEachFragment<UAttackFragment>([this, &Hit](const UAttackFragment& Fragment)
	{
		Fragment.ModifyHitSpec(*this, Hit);
	});

	return Hit;
}