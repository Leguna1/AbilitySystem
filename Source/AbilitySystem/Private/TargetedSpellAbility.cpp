#include "TargetedSpellAbility.h"

#include "AbilitySystemTags.h"
#include "CombatantComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "PayloadReceiver.h"
#include "TargetableInterface.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UTargetedSpellAbility::UTargetedSpellAbility()
{
	// The release is the point of no return, like a melee hit window opening.
	CostTrigger = EAbilityCostTrigger::OnAnimationEvent;
}

bool UTargetedSpellAbility::CanActivateAbility_Implementation() const
{
	return Super::CanActivateAbility_Implementation() && HasAbilityTarget();
}

FGameplayTag UTargetedSpellAbility::GetCostEventTag() const
{
	const FGameplayTag Configured = Super::GetCostEventTag();
	return Configured.IsValid() ? Configured : ReleaseEventTag;
}

void UTargetedSpellAbility::ActivateAbility_Implementation()
{
	ResolvedDamage = FMath::Max(
		GetModifiedFloat(AbilitySystemTags::Stat_Damage, GetRankedFloat(DamageByRank)),
		0.0f);

	LockedTarget = GetAbilityTarget();
	bReleased = false;

	// Faces the target (warp) and plays the cast montage.
	Super::ActivateAbility_Implementation();
}

void UTargetedSpellAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (bReleased || !ReleaseEventTag.IsValid() || !EventTag.MatchesTagExact(ReleaseEventTag))
	{
		return;
	}

	if (!IsCommitted() && !RequestCommit())
	{
		RequestCancelAbility();
		return;
	}

	bReleased = true;
	ReleaseSpell();
}

void UTargetedSpellAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	LockedTarget.Reset();

	Super::OnAbilityEnded_Implementation(EndReason);
}

FVector UTargetedSpellAbility::GetCastOrigin() const
{
	const ACharacter* OwnerCharacter = GetOwningCharacter();

	if (!IsValid(OwnerCharacter))
	{
		return FVector::ZeroVector;
	}

	const USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();

	return IsValid(MeshComponent) && MeshComponent->DoesSocketExist(CastSocketName)
		? MeshComponent->GetSocketLocation(CastSocketName)
		: OwnerCharacter->GetPawnViewLocation();
}

void UTargetedSpellAbility::ReleaseSpell()
{
	ACharacter* OwnerCharacter = GetOwningCharacter();
	AActor* SpellTarget = LockedTarget.Get();

	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	const FVector CastOrigin = GetCastOrigin();

	if (!IsValid(SpellTarget))
	{
		OnSpellReleased(nullptr, CastOrigin, CastOrigin, false);
		return;
	}

	const FVector ImpactLocation = SpellTarget->Implements<UTargetableInterface>()
		? ITargetableInterface::Execute_GetTargetAimLocation(SpellTarget)
		: SpellTarget->GetActorLocation();

	bool bHit = false;

	if (CanReach(SpellTarget, CastOrigin, ImpactLocation))
	{
		FHitResult SpellHit(SpellTarget, nullptr, ImpactLocation, (CastOrigin - ImpactLocation).GetSafeNormal());
		SpellHit.TraceStart = CastOrigin;
		SpellHit.TraceEnd = ImpactLocation;

		FAbilityPayload Payload;
		Payload.Damage = ResolvedDamage;
		Payload.Instigator = OwnerCharacter;
		Payload.Causer = OwnerCharacter;
		Payload.Hit = SpellHit;
		BuildHitSpec().ApplyTo(Payload);

		bHit = UCombatantComponent::DeliverPayload(SpellTarget, Payload);

		// The strike lands visibly either way; a dodge just means it didn't hurt.
		if (IsValid(ImpactEffect))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(OwnerCharacter, ImpactEffect, ImpactLocation);
		}

		if (IsValid(ImpactSound))
		{
			UGameplayStatics::PlaySoundAtLocation(OwnerCharacter, ImpactSound, ImpactLocation);
		}
	}

	OnSpellReleased(SpellTarget, CastOrigin, ImpactLocation, bHit);
}

bool UTargetedSpellAbility::CanReach(const AActor* SpellTarget, const FVector& FromLocation, const FVector& ToLocation) const
{
	const ACharacter* OwnerCharacter = GetOwningCharacter();

	if (FVector::Dist(OwnerCharacter->GetActorLocation(), SpellTarget->GetActorLocation()) > MaxReach)
	{
		return false;
	}

	if (!bRequireLineOfSight)
	{
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpellLineOfSight), false, OwnerCharacter);
	QueryParams.AddIgnoredActor(SpellTarget);

	FHitResult BlockingHit;
	return !GetWorld()->LineTraceSingleByChannel(BlockingHit, FromLocation, ToLocation, ECC_Visibility, QueryParams);
}

#if WITH_EDITOR
EDataValidationResult UTargetedSpellAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!ReleaseEventTag.IsValid())
	{
		Context.AddError(NSLOCTEXT("TargetedSpell", "NoRelease",
			"Release Event Tag is empty, so the spell never lands. Set it, and add an Ability Event notify with that tag to the cast montage."));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif