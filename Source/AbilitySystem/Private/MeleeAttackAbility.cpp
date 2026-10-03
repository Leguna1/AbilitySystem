#include "MeleeAttackAbility.h"

#include "CombatantComponent.h"
#include "PayloadReceiver.h"
#include "SwordBase.h"
#include "AbilitySystem/Public/ImpactGroupSubsystem.h"
#include "GameFramework/Character.h"
#include "AbilitySystemTags.h"



UMeleeAttackAbility::UMeleeAttackAbility()
{
	CostTrigger = EAbilityCostTrigger::OnAnimationEvent;
}

bool UMeleeAttackAbility::CanActivateAbility_Implementation() const
{
	if (!Super::CanActivateAbility_Implementation())
	{
		return false;
	}

	const ASwordBase* Sword = GetEquippedWeaponAs<ASwordBase>();
	return IsValid(Sword) && Sword->IsDrawn();
}

FGameplayTag UMeleeAttackAbility::GetCostEventTag() const
{
	const FGameplayTag Configured = Super::GetCostEventTag();
	return Configured.IsValid() ? Configured : BeginHitWindowEventTag;
}
void UMeleeAttackAbility::ActivateAbility_Implementation()
{
	// OffensiveAbilityBase handles target-facing warp + montage play.
	
	ResolvedDamage = FMath::Max(
	GetModifiedFloat(AbilitySystemTags::Stat_Damage, GetRankedFloat(DamageByRank)),
	0.0f);
	
	Super::ActivateAbility_Implementation();
}
void UMeleeAttackAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	// Ending mid-swing (cancel/interrupt) must not leave the hit box live.
	ReleaseBoundSword();

	Super::OnAbilityEnded_Implementation(EndReason);
}
void UMeleeAttackAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (BeginHitWindowEventTag.IsValid() && EventTag.MatchesTagExact(BeginHitWindowEventTag))
	{
		ASwordBase* Sword = GetEquippedWeaponAs<ASwordBase>();

		if (!IsValid(Sword))
		{
			RequestCancelAbility();
			return;
		}

		ReleaseBoundSword();

		BoundSword = Sword;
		BoundSword->OnSwordHit.AddDynamic(this, &UMeleeAttackAbility::HandleSwordHit);
		BoundSword->BeginHitDetection();
		
		if (UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this))
		{
			SwingGroup = ImpactGroups->OpenGroup(SwingImpactGroup);
		}
		
		return;
	}

	if (EndHitWindowEventTag.IsValid() && EventTag.MatchesTagExact(EndHitWindowEventTag))
	{
		ReleaseBoundSword();
	}
}

void UMeleeAttackAbility::ReleaseBoundSword()
{
	if (SwingGroup.IsValid())
	{
		if (UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this))
		{
			ImpactGroups->SealGroup(SwingGroup);
		}
		
		SwingGroup = FImpactGroupHandle();
	}
	
	if (!IsValid(BoundSword))
	{
		return;
	}
	
	BoundSword->EndHitDetection();
	BoundSword->OnSwordHit.RemoveDynamic(this, &UMeleeAttackAbility::HandleSwordHit);
	BoundSword = nullptr;
}
void UMeleeAttackAbility::HandleSwordHit(AActor* HitActor, const FHitResult& Hit)
{
	if (!IsValid(HitActor))
	{
		return;
	}

	// Resolved before delivery: a lethal payload may start destroying the target.
	const FVector ImpactLocation = !Hit.ImpactPoint.IsNearlyZero()
		? FVector(Hit.ImpactPoint)
		: HitActor->GetActorLocation();
	

	FAbilityPayload Payload;
	Payload.Damage = ResolvedDamage;
	Payload.Instigator = GetOwningCharacter();
	Payload.Causer = BoundSword.Get();
	Payload.Hit = Hit;
	Payload.SourceAbilityTags = GetAbilityTags();
	ResolveOnHitStatuses(Payload.Statuses);

	const bool bPayloadAccepted = UCombatantComponent::DeliverPayload(HitActor, Payload);

	if (UImpactGroupSubsystem* ImpactFeedback = UImpactGroupSubsystem::Get(this))
	{
		FImpactReport Report;
		Report.Group = SwingGroup;
		Report.Result = bPayloadAccepted ? EImpactResult::Hit : EImpactResult::Miss;
		Report.Location = ImpactLocation;
		Report.Rotation = IsValid(BoundSword) ? BoundSword->GetActorRotation() : FRotator::ZeroRotator;
		Report.OwnHitFeedback = SwingHitFeedback;
		Report.Hit = Hit;
		Report.HitComponent = Hit.GetComponent();

		ImpactFeedback->PlayImpact(Report);
	}
}
