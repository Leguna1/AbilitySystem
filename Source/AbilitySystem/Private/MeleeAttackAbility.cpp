// MeleeAttackAbility.cpp
#include "MeleeAttackAbility.h"

#include "PayloadReceiver.h"
#include "SwordBase.h"
#include "GameFramework/Character.h"

bool UMeleeAttackAbility::CanActivateAbility_Implementation() const
{
	if (!Super::CanActivateAbility_Implementation())
	{
		return false;
	}

	const ASwordBase* Sword = GetEquippedWeaponAs<ASwordBase>();
	return IsValid(Sword) && Sword->IsDrawn();
}
void UMeleeAttackAbility::ActivateAbility_Implementation()
{
	// OffensiveAbilityBase handles target-facing warp + montage play.
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

		// The swing becoming active is the commit point; a failed commit swings nothing.
		if (!IsCommitted() && !RequestCommit())
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
	if (!IsValid(HitActor))
	{
		return;
	}

	// Deliver a payload if the target accepts one -- same interface arrows use,
	// so melee and ranged share the damage-delivery contract.
	if (HitActor->GetClass()->ImplementsInterface(UPayloadReceiver::StaticClass()))
	{
		FAbilityPayload Payload;
		Payload.Causer = BoundSword.Get();

		IPayloadReceiver::Execute_ReceivePayload(HitActor, Payload);
	}
}
