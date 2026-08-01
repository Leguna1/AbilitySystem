#include "DirectionalDodgeAbility.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

UDirectionalDodgeAbility::UDirectionalDodgeAbility()
{
	ActivationPriority = 300;
	bCanActivateFromHeldInput = false;
	bRequireInputHeldAtResolution = false;
}

bool UDirectionalDodgeAbility::CanActivateAbility_Implementation() const
{
	if (!Super::CanActivateAbility_Implementation())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dodge] Super activation validation failed."));
		return false;
	}

	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dodge] Character invalid."));
		return false;
	}

	if (!IsValid(Character->GetCharacterMovement()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dodge] CharacterMovement invalid."));
		return false;
	}

	if (!IsValid(Character->GetCapsuleComponent()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dodge] CapsuleComponent invalid."));
		return false;
	}

	if (!bAllowAirDodge && Character->GetCharacterMovement()->IsFalling())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dodge] Rejected because character is falling."));
		return false;
	}

	const FVector2D MovementInput = GetMovementInput();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Dodge] Movement input: X=%f Y=%f"),
		MovementInput.X,
		MovementInput.Y
	);

	FVector RequestedDirection;
	bool bRequestedBackward = false;

	if (!CalculateDodgeDirection(RequestedDirection, bRequestedBackward))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Dodge] Could not calculate dodge direction."));
		return false;
	}

	FHitResult HitResult;
	const bool bPathClear = IsDodgePathClear(RequestedDirection, &HitResult);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[Dodge] Direction=%s | PathClear=%d | BlockingHit=%d | StartPenetrating=%d | HitActor=%s"),
		*RequestedDirection.ToString(),
		bPathClear,
		HitResult.bBlockingHit,
		HitResult.bStartPenetrating,
		*GetNameSafe(HitResult.GetActor())
	);

	return bPathClear;
}

void UDirectionalDodgeAbility::ActivateAbility_Implementation()
{
	ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character))
	{
		RequestCancelAbility();
		return;
	}

	FVector RequestedDirection;
	bool bRequestedBackward = false;

	if (!CalculateDodgeDirection(RequestedDirection, bRequestedBackward) ||
		!IsDodgePathClear(RequestedDirection))
	{
		RequestCancelAbility();
		return;
	}

	DodgeDirection = RequestedDirection;
	bDodgeBackward = bRequestedBackward;

	if (bStopMovementBeforeDodge && IsValid(Character->GetCharacterMovement()))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}

	if (bRotateTowardDodgeDirection && !bDodgeBackward)
	{
		const float DodgeYaw = DodgeDirection.Rotation().Yaw;
		Character->SetActorRotation(FRotator(0.0f, DodgeYaw, 0.0f));
	}

	if (!RequestCommit())
	{
		RequestCancelAbility();
		return;
	}

	OnDodgePrepared(DodgeDirection);

	// Feedback: start burst (world or attached) + ongoing trail (tracked so we
	// can stop it when the dodge ends). Captured start location anchors both the
	// start burst and, later, comparison for the landing burst.
	DodgeStartLocation = Character->GetActorLocation();

	PlayDodgeFeedbackSet(StartFeedback, DodgeStartLocation, /*bTrackOngoing*/ false);
	OnDodgeStartFeedback(DodgeDirection, DodgeStartLocation);

	PlayDodgeFeedbackSet(OngoingFeedback, DodgeStartLocation, /*bTrackOngoing*/ true);
	OnDodgeOngoingFeedback(DodgeDirection);

	UAnimMontage* Montage = SelectAbilityMontage();

	if (!PlayAbilityMontage(Montage, MontagePlayRate))
	{
		RequestCancelAbility();
		return;
	}

	if (UAnimInstance* AnimInstance = GetAnimInstance())
	{
		const FName StartSection = bDodgeBackward ? BackwardDodgeSection : ForwardDodgeSection;
		AnimInstance->Montage_JumpToSection(StartSection, Montage);
	}
}

bool UDirectionalDodgeAbility::CanReplaceActiveAbility_Implementation(const UAbility* CurrentAbility) const
{
	return true;
}

bool UDirectionalDodgeAbility::CalculateDodgeDirection(FVector& OutDirection, bool& bOutDodgeBackward) const
{
	OutDirection = FVector::ZeroVector;
	bOutDodgeBackward = false;

	const ACharacter* Character = GetOwningCharacter();

	if (!IsValid(Character))
	{
		return false;
	}

	const FVector2D MovementInput = GetMovementInput().GetClampedToMaxSize(1.0f);

	if (MovementInput.IsNearlyZero())
	{
		OutDirection = -Character->GetActorForwardVector().GetSafeNormal2D();
		bOutDodgeBackward = true;
		return !OutDirection.IsNearlyZero();
	}

	float ReferenceYaw = Character->GetActorRotation().Yaw;

	if (const AController* Controller = Character->GetController())
	{
		ReferenceYaw = Controller->GetControlRotation().Yaw;
	}

	const FRotator YawRotation(0.0f, ReferenceYaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	OutDirection = ForwardDirection * MovementInput.Y + RightDirection * MovementInput.X;
	OutDirection.Z = 0.0f;
	OutDirection.Normalize();

	return !OutDirection.IsNearlyZero();
}

bool UDirectionalDodgeAbility::IsDodgePathClear(const FVector& Direction, FHitResult* OutHit) const
{
	const ACharacter* Character = GetOwningCharacter();
	const UCapsuleComponent* CapsuleComponent = IsValid(Character)
		? Character->GetCapsuleComponent()
		: nullptr;

	if (!IsValid(Character) ||
		!IsValid(CapsuleComponent) ||
		Direction.IsNearlyZero() ||
		!IsValid(GetWorld()))
	{
		return false;
	}

	const float CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();

	const FVector Start = Character->GetActorLocation();
	const float TraceDistance = DodgeDistance + ObstaclePadding;
	const FVector End = Start + Direction.GetSafeNormal2D() * TraceDistance;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DirectionalDodge), false, Character);
	QueryParams.AddIgnoredActor(Character);

	FHitResult HitResult;

	const bool bBlocked = GetWorld()->SweepSingleByChannel(
		HitResult,
		Start,
		End,
		FQuat::Identity,
		DodgeTraceChannel,
		FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
		QueryParams
	);

	if (OutHit)
	{
		*OutHit = HitResult;
	}

	return !bBlocked;
}

void UDirectionalDodgeAbility::OnDodgePrepared_Implementation(FVector Direction)
{
}
FVector UDirectionalDodgeAbility::GetRootMotionWarpDirection_Implementation() const
{
	// Dodge already resolved its direction (including backward) before the montage
	// plays, so warp translation follows it rather than actor forward.
	if (!DodgeDirection.IsNearlyZero())
	{
		return DodgeDirection.GetSafeNormal2D();
	}

	return Super::GetRootMotionWarpDirection_Implementation();
}

void UDirectionalDodgeAbility::OnAbilityEnded_Implementation(const EAbilityEndReason EndReason)
{
	// Stop the attached trail, then fire the landing burst at the current location.
	StopOngoingDodgeFeedback();

	const ACharacter* Character = GetOwningCharacter();
	const FVector EndLocation = IsValid(Character)
		? Character->GetActorLocation()
		: DodgeStartLocation;

	PlayDodgeFeedbackSet(EndFeedback, EndLocation, /*bTrackOngoing*/ false);
	OnDodgeEndFeedback(EndLocation);

	Super::OnAbilityEnded_Implementation(EndReason);
}

void UDirectionalDodgeAbility::PlayDodgeFeedbackSet(
	const FDodgeFeedbackSet& Set,
	const FVector& WorldLocation,
	const bool bTrackOngoing)
{
	ACharacter* Character = GetOwningCharacter();
	if (!IsValid(Character))
	{
		return;
	}

	USceneComponent* AttachRoot = Character->GetRootComponent();

	// --- Effect ---
	if (IsValid(Set.Effect))
	{
		UNiagaraComponent* SpawnedEffect = nullptr;

		if (Set.bAttachToCharacter && IsValid(AttachRoot))
		{
			SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
				Set.Effect,
				AttachRoot,
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget,
				/*bAutoDestroy*/ !bTrackOngoing
			);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				Character,
				Set.Effect,
				WorldLocation,
				Character->GetActorRotation()
			);
		}

		if (bTrackOngoing && IsValid(SpawnedEffect))
		{
			OngoingEffectComponent = SpawnedEffect;
		}
	}

	// --- Sound ---
	if (IsValid(Set.Sound))
	{
		if (Set.bAttachToCharacter && IsValid(AttachRoot))
		{
			UAudioComponent* SpawnedSound = UGameplayStatics::SpawnSoundAttached(
				Set.Sound,
				AttachRoot
			);

			if (bTrackOngoing && IsValid(SpawnedSound))
			{
				OngoingSoundComponent = SpawnedSound;
			}
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(
				Character,
				Set.Sound,
				WorldLocation
			);
		}
	}
}

void UDirectionalDodgeAbility::StopOngoingDodgeFeedback()
{
	if (IsValid(OngoingEffectComponent))
	{
		OngoingEffectComponent->Deactivate();
		OngoingEffectComponent = nullptr;
	}

	if (IsValid(OngoingSoundComponent))
	{
		OngoingSoundComponent->Stop();
		OngoingSoundComponent = nullptr;
	}
}