#include "MeleeAttackAbility.h"

#include "CombatantComponent.h"
#include "PayloadReceiver.h"
#include "SwordBase.h"
#include "FeedbackComponent.h"
#include "FeedbackSubsystem.h"
#include "FeedbackTags.h"
#include "GameFramework/Character.h"
#include "ImpactGroupSubsystem.h"
#include "WeaponBase.h"
#include "AbilitySystemTags.h"
#include "MeleeTraceComponent.h"



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

	if (HitSource == EMeleeHitSource::Body)
	{
		const ACharacter* Character = GetOwningCharacter();
		return IsValid(Character) && IsValid(Character->FindComponentByClass<UMeleeTraceComponent>());
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
	
	if (HitSource == EMeleeHitSource::Body)
	{
		BindBodyTrace();
	}
	
	Super::ActivateAbility_Implementation();
}
void UMeleeAttackAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	// Ending mid-swing (cancel/interrupt) must not leave the hit box live.
	ReleaseBoundSword();
	ReleaseBodyTrace();

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
		return;
	}

	if (EndHitWindowEventTag.IsValid() && EventTag.MatchesTagExact(EndHitWindowEventTag))
	{
		ReleaseBoundSword();
	}
}

void UMeleeAttackAbility::ReleaseBoundSword()
{
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
	ProcessMeleeHit(HitActor, Hit, BoundSword.Get());
}

void UMeleeAttackAbility::HandleBodyHit(AActor* HitActor, const FHitResult& Hit, FName ShapeName, UAnimSequenceBase* SourceAnimation)
{
	// A blending-out montage from the previous attack can't land hits for this one.
	if (AcceptsAnimationEvent(SourceAnimation))
	{
		ProcessMeleeHit(HitActor, Hit, GetOwningCharacter());
	}
}

void UMeleeAttackAbility::ProcessMeleeHit(AActor* HitActor, const FHitResult& Hit, AActor* Causer)
{
	if (!IsValid(HitActor))
	{
		return;
	}

	// Resolved before delivery: a lethal payload may start destroying the target.
	AWeaponBase* Weapon = Cast<AWeaponBase>(Causer);

	const FVector StrikeLocation = IsValid(Weapon)
		? Weapon->ResolveStrikeLocation(HitActor, Hit)
		: UFeedbackSubsystem::ResolveStrikeLocation(HitActor, Hit, IsValid(Causer) ? Causer->GetActorLocation() : HitActor->GetActorLocation());

	FAbilityPayload Payload;
	Payload.Damage = ResolvedDamage;
	Payload.Instigator = GetOwningCharacter();
	Payload.Causer = Causer;
	Payload.Hit = Hit;
	BuildHitSpec().ApplyTo(Payload);

	const bool bPayloadAccepted = UCombatantComponent::DeliverPayload(HitActor, Payload);

	if (IsValid(Weapon))
	{
		Weapon->PlayHitFeedback(HitActor, Hit, StrikeLocation, bPayloadAccepted, GetImpactGroup());
	}
	else
	{
		PlayBodyHitFeedback(HitActor, Hit, StrikeLocation, bPayloadAccepted, Causer);
	}
}

void UMeleeAttackAbility::PlayBodyHitFeedback(AActor* HitActor, const FHitResult& Hit, const FVector& StrikeLocation, const bool bHitTarget, AActor* Causer) const
{
	UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this);

	if (!IsValid(ImpactGroups))
	{
		return;
	}

	const UFeedbackComponent* AttackerFeedback = IsValid(Causer) ? Causer->FindComponentByClass<UFeedbackComponent>() : nullptr;

	FImpactReport Report;
	Report.Group = GetImpactGroup();
	Report.Result = bHitTarget ? EImpactResult::Hit : EImpactResult::Miss;
	Report.Location = StrikeLocation;
	Report.Rotation = IsValid(Causer)
		? (StrikeLocation - Causer->GetActorLocation()).GetSafeNormal().Rotation()
		: FRotator::ZeroRotator;

	Report.FeedbackSet = IsValid(AttackerFeedback) ? AttackerFeedback->GetActiveSet() : nullptr;
	Report.FallbackSet = IsValid(AttackerFeedback) ? AttackerFeedback->GetFallbackSet() : nullptr;
	Report.HitCue = FeedbackTags::Cue_Weapon_Hit;
	Report.MissCue = FeedbackTags::Cue_Weapon_Miss;
	Report.Instigator = GetOwningCharacter();
	Report.Target = HitActor;
	Report.Hit = Hit;

	if (IsValid(Hit.GetComponent()))
	{
		Report.HitComponent = Hit.GetComponent();
	}
	else if (IsValid(HitActor))
	{
		Report.HitComponent = Cast<UPrimitiveComponent>(HitActor->GetRootComponent());
	}

	ImpactGroups->PlayImpact(Report);
}

void UMeleeAttackAbility::BindBodyTrace()
{
	const ACharacter* Character = GetOwningCharacter();
	BodyTrace = IsValid(Character) ? Character->FindComponentByClass<UMeleeTraceComponent>() : nullptr;

	if (!IsValid(BodyTrace))
	{
		RequestCancelAbility();
		return;
	}

	BodyTrace->OnTraceStarted.AddDynamic(this, &UMeleeAttackAbility::HandleBodyTraceStarted);
	BodyTrace->OnMeleeHit.AddDynamic(this, &UMeleeAttackAbility::HandleBodyHit);
}

void UMeleeAttackAbility::ReleaseBodyTrace()
{
	if (!IsValid(BodyTrace))
	{
		return;
	}

	// Ending mid-swing must not leave the fists live.
	BodyTrace->EndAllTraces();

	BodyTrace->OnTraceStarted.RemoveDynamic(this, &UMeleeAttackAbility::HandleBodyTraceStarted);
	BodyTrace->OnMeleeHit.RemoveDynamic(this, &UMeleeAttackAbility::HandleBodyHit);
	BodyTrace = nullptr;
}

void UMeleeAttackAbility::HandleBodyTraceStarted(UAnimSequenceBase* SourceAnimation)
{
	if (!AcceptsAnimationEvent(SourceAnimation))
	{
		return;
	}

	// The strike becoming active is the point of no return, as with a sword swing.
	if (!IsCommitted() && !RequestCommit())
	{
		RequestCancelAbility();
	}
}
