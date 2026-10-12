#include "ChanneledAreaSpellAbility.h"

#include "AbilitySystemTags.h"
#include "CombatantComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "PayloadReceiver.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UChanneledAreaSpellAbility::CanActivateAbility_Implementation() const
{
	return Super::CanActivateAbility_Implementation() && HasAbilityTarget();
}

void UChanneledAreaSpellAbility::ActivateAbility_Implementation()
{
	ResolvedDamage = FMath::Max(
		GetModifiedFloat(AbilitySystemTags::Stat_Damage, GetRankedFloat(DamageByRank)),
		0.0f);

	bReleased = false;
	AreaCenter = FindGroundBelow(GetAbilityTarget());

	// Faces the target (warp) and plays the channel montage.
	Super::ActivateAbility_Implementation();

	if (GetAbilityStatus() != EAbilityStatus::Active)
	{
		return;
	}

	// Committed at cast start: the cooldown runs even if she's interrupted, so interrupting is a real win.
	if (!IsCommitted() && !RequestCommit())
	{
		RequestCancelAbility();
		return;
	}

	if (IsValid(TelegraphEffect))
	{
		TelegraphComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetOwningCharacter(), TelegraphEffect, AreaCenter);

		if (IsValid(TelegraphComponent) && !TelegraphRadiusParameter.IsNone())
		{
			TelegraphComponent->SetVariableFloat(TelegraphRadiusParameter, AreaRadius);
		}
	}

	OnChannelStarted(AreaCenter, AreaRadius);
}

void UChanneledAreaSpellAbility::OnAnimationEvent_Implementation(const FGameplayTag EventTag)
{
	Super::OnAnimationEvent_Implementation(EventTag);

	if (bReleased || !ReleaseEventTag.IsValid() || !EventTag.MatchesTagExact(ReleaseEventTag))
	{
		return;
	}

	bReleased = true;
	ReleaseArea();
}

void UChanneledAreaSpellAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	RemoveTelegraph();

	if (!bReleased)
	{
		OnChannelInterrupted(AreaCenter);
	}

	Super::OnAbilityEnded_Implementation(EndReason);
}

FVector UChanneledAreaSpellAbility::FindGroundBelow(const AActor* SpellTarget) const
{
	if (!IsValid(SpellTarget))
	{
		return FVector::ZeroVector;
	}

	const FVector TraceStart = SpellTarget->GetActorLocation();
	const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, 1000.0f);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AreaSpellGround), false, GetOwningCharacter());
	QueryParams.AddIgnoredActor(SpellTarget);

	FHitResult GroundHit;

	if (GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		return GroundHit.ImpactPoint;
	}

	// No ground found: use the bottom of the target's collision.
	float TargetRadius = 0.0f;
	float TargetHalfHeight = 0.0f;
	SpellTarget->GetSimpleCollisionCylinder(TargetRadius, TargetHalfHeight);

	return TraceStart - FVector(0.0f, 0.0f, TargetHalfHeight);
}

void UChanneledAreaSpellAbility::ReleaseArea()
{
	ACharacter* OwnerCharacter = GetOwningCharacter();

	RemoveTelegraph();

	if (!IsValid(OwnerCharacter))
	{
		return;
	}

	// A box around the disc finds candidates; the 2D distance check makes the area round.
	const FVector BoxCenter = AreaCenter + FVector(0.0f, 0.0f, AreaHeight * 0.5f);
	const FCollisionShape AreaBox = FCollisionShape::MakeBox(FVector(AreaRadius, AreaRadius, AreaHeight * 0.5f));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AreaSpellRelease), false, OwnerCharacter);
	TArray<FOverlapResult> Overlaps;

	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		BoxCenter,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		AreaBox,
		QueryParams);

	const FHitSpec HitSpec = BuildHitSpec();
	TSet<AActor*> AlreadyHit;
	int32 TargetsHit = 0;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* OverlapActor = Overlap.GetActor();

		if (!IsValid(OverlapActor) || AlreadyHit.Contains(OverlapActor))
		{
			continue;
		}

		AlreadyHit.Add(OverlapActor);

		// Inside the circle if any part of its collision is.
		const float Reach = AreaRadius + OverlapActor->GetSimpleCollisionRadius();

		if (FVector::Dist2D(OverlapActor->GetActorLocation(), AreaCenter) > Reach)
		{
			continue;
		}

		FHitResult AreaHit(OverlapActor, Overlap.GetComponent(), OverlapActor->GetActorLocation(), FVector::UpVector);
		AreaHit.TraceStart = AreaCenter;
		AreaHit.TraceEnd = OverlapActor->GetActorLocation();

		FAbilityPayload Payload;
		Payload.Damage = ResolvedDamage;
		Payload.Instigator = OwnerCharacter;
		Payload.Causer = OwnerCharacter;
		Payload.Hit = AreaHit;
		HitSpec.ApplyTo(Payload);

		// Allies and the caster are refused by the receiver's team check.
		if (UCombatantComponent::DeliverPayload(OverlapActor, Payload))
		{
			++TargetsHit;
		}
	}

	if (IsValid(ReleaseEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(OwnerCharacter, ReleaseEffect, AreaCenter);
	}

	if (IsValid(ReleaseSound))
	{
		UGameplayStatics::PlaySoundAtLocation(OwnerCharacter, ReleaseSound, AreaCenter);
	}

	OnAreaReleased(AreaCenter, AreaRadius, TargetsHit);
}

void UChanneledAreaSpellAbility::RemoveTelegraph()
{
	if (IsValid(TelegraphComponent))
	{
		TelegraphComponent->DestroyComponent();
	}

	TelegraphComponent = nullptr;
}

#if WITH_EDITOR
EDataValidationResult UChanneledAreaSpellAbility::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!ReleaseEventTag.IsValid())
	{
		Context.AddError(NSLOCTEXT("ChanneledAreaSpell", "NoRelease",
			"Release Event Tag is empty, so the area never goes off. Set it, and add an Ability Event notify with that tag at the end of the channel."));
		Result = EDataValidationResult::Invalid;
	}

	if (!IsValid(TelegraphEffect))
	{
		Context.AddWarning(NSLOCTEXT("ChanneledAreaSpell", "NoTelegraph",
			"No Telegraph Effect: the player gets no warning where the area will hit."));
	}

	return Result;
}
#endif