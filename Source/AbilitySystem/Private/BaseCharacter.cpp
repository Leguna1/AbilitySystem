#include "BaseCharacter.h"

#include "AbilityComponent.h"
#include "AttributeComponent.h"
#include "CombatantComponent.h"
#include "ResourceComponent.h"
#include "Components/CapsuleComponent.h"

void ABaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Every component exists by now, whether a C++ child or a Blueprint added it.
	AbilityComponent = FindComponentByClass<UAbilityComponent>();
	CombatantComponent = FindComponentByClass<UCombatantComponent>();
	ResourceComponent = FindComponentByClass<UResourceComponent>();
	AttributeComponent = FindComponentByClass<UAttributeComponent>();
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnDied.AddDynamic(this, &ABaseCharacter::HandleCombatantDied);
	}
}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(CombatantComponent))
	{
		CombatantComponent->OnDied.RemoveDynamic(this, &ABaseCharacter::HandleCombatantDied);
	}

	Super::EndPlay(EndPlayReason);
}

void ABaseCharacter::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();

	OutLocation = IsValid(MeshComponent) && MeshComponent->DoesSocketExist(EyeSocketName)
		? MeshComponent->GetSocketLocation(EyeSocketName)
		: GetPawnViewLocation();

	OutRotation = FRotator(0.0f, GetActorRotation().Yaw, 0.0f);
}

FVector ABaseCharacter::GetTargetAimLocation_Implementation() const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();

	return IsValid(MeshComponent) && MeshComponent->DoesSocketExist(AimSocketName)
		? MeshComponent->GetSocketLocation(AimSocketName)
		: GetActorLocation();
}

bool ABaseCharacter::IsDead() const
{
	return IsValid(CombatantComponent) && CombatantComponent->IsDead();
}

void ABaseCharacter::HandleCombatantDied(AActor* Killer)
{
	HandleCharacterDied(Killer);
}

void ABaseCharacter::HandleCharacterDied(AActor* Killer)
{
	OnCharacterDied(Killer);
}
float ABaseCharacter::GetEdgeDistanceTo(const AActor* Other) const
{
	if (!IsValid(Other))
	{
		return TNumericLimits<float>::Max();
	}

	// Measured flat, so a tall golem and a short player aren't kept apart by their height difference.
	const float CenterDistance = FVector::Dist2D(GetActorLocation(), Other->GetActorLocation());

	const ACharacter* OtherCharacter = Cast<ACharacter>(Other);
	const float OtherRadius = IsValid(OtherCharacter)
		? OtherCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius()
		: 0.0f;

	return CenterDistance - GetCapsuleComponent()->GetScaledCapsuleRadius() - OtherRadius;
}