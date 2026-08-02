// MeleeAttackAbility.cpp
#include "MeleeAttackAbility.h"

#include "PayloadReceiver.h"
#include "SwordBase.h"
#include "WeaponComponent.h"
#include "GameFramework/Character.h"

void UMeleeAttackAbility::ActivateAbility_Implementation()
{
	// OffensiveAbilityBase handles target-facing warp + montage play.
	Super::ActivateAbility_Implementation();
}

void UMeleeAttackAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	// Safety: if the ability ends mid-swing (cancel/interrupt), make sure the
	// sword's hit detection is turned off and we unbind.
	if (UWeaponComponent* Weapon = GetWeaponComponent())
	{
		Weapon->EndWeaponHitDetection();
	}

	if (IsValid(BoundSword))
	{
		BoundSword->OnSwordHit.RemoveDynamic(this, &UMeleeAttackAbility::HandleSwordHit);
		BoundSword = nullptr;
	}

	Super::OnAbilityEnded_Implementation(EndReason);
}

void UMeleeAttackAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	UWeaponComponent* Weapon = GetWeaponComponent();
	if (!Weapon)
	{
		return;
	}

	if (BeginHitWindowEventTag.IsValid() && EventTag.MatchesTagExact(BeginHitWindowEventTag))
	{
		// The swing becoming active is the commit point: spend cost, start cooldown.
		if (!IsCommitted())
		{
			RequestCommit();
		}

		// Bind to the sword's hit reports for this swing, then open the window.
		if (ASwordBase* Sword = Weapon->GetWeapon())
		{
			BoundSword = Sword;
			Sword->OnSwordHit.RemoveDynamic(this, &UMeleeAttackAbility::HandleSwordHit);
			Sword->OnSwordHit.AddDynamic(this, &UMeleeAttackAbility::HandleSwordHit);
		}

		Weapon->BeginWeaponHitDetection();
	}
	else if (EndHitWindowEventTag.IsValid() && EventTag.MatchesTagExact(EndHitWindowEventTag))
	{
		Weapon->EndWeaponHitDetection();

		if (IsValid(BoundSword))
		{
			BoundSword->OnSwordHit.RemoveDynamic(this, &UMeleeAttackAbility::HandleSwordHit);
			BoundSword = nullptr;
		}
	}
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
		Payload.Damage = Damage;
		Payload.Instigator = GetOwningCharacter();
		Payload.Causer = IsValid(BoundSword) ? Cast<AActor>(BoundSword) : nullptr;
		Payload.Hit = Hit;

		IPayloadReceiver::Execute_ReceivePayload(HitActor, Payload);
	}
}

UWeaponComponent* UMeleeAttackAbility::GetWeaponComponent() const
{
	const ACharacter* Character = GetOwningCharacter();
	return IsValid(Character) ? Character->FindComponentByClass<UWeaponComponent>() : nullptr;
}