#include "DirectionalDodgeAbility.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"


UDirectionalDodgeAbility::UDirectionalDodgeAbility()
{
	ActivationPriority = 300;
	bCanActivateFromHeldInput = false;
	bRequireInputHeldAtResolution = false;
	CostTrigger = EAbilityCostTrigger::OnActivate;
	// A stagger lasts as long as it lasts, whatever buffs or debuffs the character has.
	bAffectedByAttackSpeed = false;
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
		Character->SetActorRotation(FRotator(0.0f, DodgeDirection.Rotation().Yaw, 0.0f));
	}

	OnDodgePrepared(DodgeDirection);

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

bool UDirectionalDodgeAbility::CanReplaceActiveAbility_Implementation(const UActiveAbility* CurrentAbility) const
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
	const UCapsuleComponent* CapsuleComponent = IsValid(Character) ? Character->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* Movement = IsValid(Character) ? Character->GetCharacterMovement() : nullptr;

	if (!IsValid(CapsuleComponent) ||
		!IsValid(Movement) ||
		Direction.IsNearlyZero() ||
		!IsValid(GetWorld()))
	{
		return false;
	}

	const float CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
	const float CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
	const float StepHeight = Movement->MaxStepHeight;
	const float WalkableFloorZ = Movement->GetWalkableFloorZ();

	// Raise the capsule's bottom by the step height while keeping its top in place,
	// so the floor and small steps don't count as obstacles.
	const float Lift = FMath::Min(StepHeight * 0.5f, FMath::Max(CapsuleHalfHeight - CapsuleRadius, 0.0f));
	const FCollisionShape SweepShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight - Lift);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DirectionalDodge), false, Character);

	const FVector FlatDirection = Direction.GetSafeNormal2D();
	FVector Position = Character->GetActorLocation() + FVector(0.0f, 0.0f, Lift);
	float Remaining = DodgeDistance + ObstaclePadding;

	constexpr int32 MaxSlopeSteps = 4;

	for (int32 StepIndex = 0; StepIndex < MaxSlopeSteps; ++StepIndex)
	{
		FHitResult HitResult;

		const bool bBlocked = GetWorld()->SweepSingleByChannel(
			HitResult,
			Position,
			Position + FlatDirection * Remaining,
			FQuat::Identity,
			DodgeTraceChannel,
			SweepShape,
			QueryParams
		);

		if (OutHit)
		{
			*OutHit = HitResult;
		}

		if (!bBlocked)
		{
			return true;
		}

		// Walls, and starting inside geometry, reject the dodge.
		if (HitResult.bStartPenetrating || HitResult.ImpactNormal.Z < WalkableFloorZ)
		{
			return false;
		}

		// Walkable rise: step up onto it and continue with what's left.
		Remaining -= HitResult.Distance;

		if (Remaining <= UE_KINDA_SMALL_NUMBER)
		{
			return true;
		}

		Position = HitResult.Location + FVector(0.0f, 0.0f, StepHeight);
	}

	// Still climbing after every step: treat as too steep.
	return false;
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