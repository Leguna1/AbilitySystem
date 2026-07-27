#include "BowAnimInstance.h"

#include "BowBase.h"

void UBowAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningBow = Cast<ABowBase>(GetOwningActor());
}

void UBowAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!IsValid(OwningBow))
	{
		OwningBow = Cast<ABowBase>(GetOwningActor());
	}

	if (!IsValid(OwningBow))
	{
		DrawAlpha = 0.0f;
		StringTargetLocation = FVector::ZeroVector;
		return;
	}

	DrawAlpha = OwningBow->GetDrawAlpha();
	StringTargetLocation = OwningBow->GetStringTargetLocation();
}