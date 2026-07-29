#include "ArrowBase.h"

#include "ArrowDataAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

AArrowBase::AArrowBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	SetRootComponent(HitBox);

	HitBox->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	HitBox->SetGenerateOverlapEvents(true);

	KillCam = CreateDefaultSubobject<UCameraComponent>(TEXT("KillCam"));
	KillCam->SetupAttachment(HitBox);

	ArrowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArrowMesh"));
	ArrowMesh->SetupAttachment(HitBox);
	ArrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TipLocation = CreateDefaultSubobject<USceneComponent>(TEXT("TipLocation"));
	TipLocation->SetupAttachment(ArrowMesh);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = HitBox;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f;
	ProjectileMovement->Velocity = FVector::ZeroVector;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bAutoActivate = false;
}

void AArrowBase::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(HitBox))
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitBox->OnComponentBeginOverlap.AddDynamic(
			this,
			&AArrowBase::HandleHitBoxBeginOverlap
		);
	}

	if (IsValid(ArrowMesh))
	{
		DefaultArrowMeshRotation = ArrowMesh->GetRelativeRotation();
	}

	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	ResetForPool();
}

void AArrowBase::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsSpinning || !IsValid(ArrowMesh))
	{
		return;
	}

	SpinElapsedTime += FMath::Max(DeltaTime, 0.0f);

	const float Alpha = SpinDuration > 0.0f
		? FMath::Clamp(SpinElapsedTime / SpinDuration, 0.0f, 1.0f)
		: 1.0f;

	FRotator NewRotation = SpinInitialRotation;
	NewRotation.Roll += FMath::Lerp(0.0f, SpinDegrees, Alpha);

	ArrowMesh->SetRelativeRotation(NewRotation);

	if (Alpha >= 1.0f)
	{
		bIsSpinning = false;
		SetActorTickEnabled(false);
	}
}

float AArrowBase::GetCalculatedDamage() const
{
	if (!IsValid(ArrowData))
	{
		return 0.0f;
	}

	const float StrengthMultiplier = FMath::Lerp(
		1.0f,
		ArrowData->MaximumDamageMultiplier,
		FiredStrength
	);

	return ArrowData->BaseDamage * StrengthMultiplier;
}

void AArrowBase::SpinBegin_Implementation()
{
	if (!IsValid(ArrowMesh))
	{
		return;
	}

	SpinInitialRotation = ArrowMesh->GetRelativeRotation();
	SpinElapsedTime = 0.0f;
	bIsSpinning = true;

	SetActorTickEnabled(true);
}

bool AArrowBase::Fire_Implementation(const FVector& Direction, const float Strength, const bool bTargetedShot)
{
	if (!IsValid(ArrowData) ||
		!IsValid(ProjectileMovement) ||
		bIsInFlight)
	{
		return false;
	}

	const FVector NormalizedDirection = Direction.GetSafeNormal();

	if (NormalizedDirection.IsNearlyZero())
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	StopOngoingFeedback();

	FiredStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	bWasTargetedShot = bTargetedShot;
	bIsInFlight = true;
	bHasImpacted = false;

	const float Speed = FMath::Lerp(
		ArrowData->MinimumSpeed,
		ArrowData->MaximumSpeed,
		FiredStrength
	);

	const float GravityScale = FMath::Lerp(
		ArrowData->MaximumGravityScale,
		ArrowData->MinimumGravityScale,
		FiredStrength
	);

	if (Speed <= KINDA_SMALL_NUMBER)
	{
		bIsInFlight = false;
		return false;
	}

	SetActorRotation(NormalizedDirection.Rotation());

	Velocity = NormalizedDirection * Speed;

	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = Velocity;
	ProjectileMovement->ProjectileGravityScale = GravityScale;
	ProjectileMovement->Activate(true);

	if (IsValid(HitBox))
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	if (!bWasTargetedShot &&
		ArrowData->MaximumUntargetedTravelDistance > 0.0f)
	{
		ScheduleFlightExpiry(
			ArrowData->MaximumUntargetedTravelDistance / Speed
		);
	}
	else
	{
		ScheduleFlightExpiry(ArrowData->TargetedFlightLifespan);
	}

	SpinBegin();
	
	StartOngoingFeedback();

	return true;
}

bool AArrowBase::ActivateFromPool(UArrowDataAsset* NewArrowData)
{
	if (!IsValid(NewArrowData) || !IsValid(NewArrowData->ArrowMesh))
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	StopOngoingFeedback();

	if (GetAttachParentActor() != nullptr ||
		(IsValid(GetRootComponent()) &&
			GetRootComponent()->GetAttachParent() != nullptr))
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	ArrowData = NewArrowData;
	FiredStrength = 0.0f;
	Velocity = FVector::ZeroVector;

	bIsInFlight = false;
	bHasImpacted = false;
	bWasTargetedShot = false;
	bIsSpinning = false;
	SpinElapsedTime = 0.0f;

	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}

	if (IsValid(HitBox))
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (IsValid(ArrowMesh))
	{
		ArrowMesh->SetStaticMesh(ArrowData->ArrowMesh);
		ArrowMesh->SetRelativeRotation(DefaultArrowMeshRotation);
	}

	SetActorTickEnabled(false);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	return true;
}

void AArrowBase::ResetForPool()
{
	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	StopOngoingFeedback();

	if (GetAttachParentActor() != nullptr ||
		(IsValid(GetRootComponent()) &&
			GetRootComponent()->GetAttachParent() != nullptr))
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}

	if (IsValid(HitBox))
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (IsValid(ArrowMesh))
	{
		ArrowMesh->SetRelativeRotation(DefaultArrowMeshRotation);
	}

	ArrowData = nullptr;
	FiredStrength = 0.0f;
	Velocity = FVector::ZeroVector;

	bIsInFlight = false;
	bHasImpacted = false;
	bWasTargetedShot = false;
	bIsSpinning = false;
	SpinElapsedTime = 0.0f;

	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AArrowBase::ReturnToPool()
{
	ResetForPool();
	OnReadyToRecycle.Broadcast(this);
}

bool AArrowBase::Redirect(const FVector& NewDirection)
{
	if (!IsInFlight() || !IsValid(ProjectileMovement))
	{
		return false;
	}

	const FVector NormalizedDirection = NewDirection.GetSafeNormal();

	if (NormalizedDirection.IsNearlyZero())
	{
		return false;
	}

	const float CurrentSpeed = ProjectileMovement->Velocity.Size();

	if (CurrentSpeed <= UE_KINDA_SMALL_NUMBER)
	{
		return false;
	}

	Velocity = NormalizedDirection * CurrentSpeed;

	ProjectileMovement->Velocity = Velocity;
	SetActorRotation(Velocity.Rotation());

	return true;
}

bool AArrowBase::IsInFlight() const
{
	return bIsInFlight &&
		!bHasImpacted &&
		IsValid(ProjectileMovement) &&
		ProjectileMovement->IsActive();
}

void AArrowBase::HandleHitBoxBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bIsInFlight ||
		bHasImpacted ||
		!IsValid(OtherActor) ||
		OtherActor == this ||
		OtherActor == GetOwner() ||
		OtherActor == GetInstigator())
	{
		return;
	}

	UPrimitiveComponent* HitComponent = SweepResult.GetComponent();

	if (!IsValid(HitComponent))
	{
		HitComponent = OtherComponent;
	}

	HandleImpact(
		OtherActor,
		HitComponent,
		bFromSweep,
		SweepResult
	);
}

void AArrowBase::HandleImpact(
	AActor* HitActor,
	UPrimitiveComponent* HitComponent,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bHasImpacted ||
		!IsValid(HitActor) ||
		!IsValid(ArrowData))
	{
		return;
	}

	bHasImpacted = true;
	bIsInFlight = false;
	bIsSpinning = false;

	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	StopOngoingFeedback();

	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	if (IsValid(HitBox))
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetActorTickEnabled(false);

	if (IsValid(HitComponent))
	{
		const FAttachmentTransformRules AttachmentRules(
			EAttachmentRule::KeepWorld,
			EAttachmentRule::KeepWorld,
			EAttachmentRule::KeepWorld,
			true
		);

		AttachToComponent(
			HitComponent,
			AttachmentRules
		);
	}

	FVector ImpactLocation = GetActorLocation();

	if (bFromSweep && !SweepResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(SweepResult.ImpactPoint);
	}

	PlayEndFeedback(ImpactLocation);

	if (IsValid(HitComponent) &&
		HitComponent->IsSimulatingPhysics())
	{
		HitComponent->AddImpulseAtLocation(
			Velocity * ArrowData->ImpactImpulseMultiplier,
			ImpactLocation
		);
	}

	OnArrowHit.Broadcast(
		this,
		HitActor,
		GetCalculatedDamage(),
		SweepResult
	);

	ScheduleRecycle(ArrowData->ImpactLifespan);
}

void AArrowBase::PlayStartFeedback()
{
	if (!IsValid(ArrowData))
	{
		return;
	}

	const FVector FeedbackLocation = IsValid(TipLocation)
		? TipLocation->GetComponentLocation()
		: GetActorLocation();

	if (IsValid(ArrowData->StartSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ArrowData->StartSound,
			FeedbackLocation
		);
	}

	if (IsValid(ArrowData->StartEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ArrowData->StartEffect,
			FeedbackLocation,
			GetActorRotation()
		);
	}
}

void AArrowBase::StartOngoingFeedback()
{
	if (!IsValid(ArrowData))
	{
		return;
	}

	USceneComponent* AttachComponent = GetRootComponent();

	if (IsValid(TipLocation))
	{
		AttachComponent = TipLocation.Get();
	}

	if (!IsValid(AttachComponent))
	{
		return;
	}

	if (IsValid(ArrowData->OngoingSound))
	{
		OngoingSoundRef = UGameplayStatics::SpawnSoundAttached(
			ArrowData->OngoingSound,
			AttachComponent
		);
	}

	if (IsValid(ArrowData->OngoingEffect))
	{
		OngoingEffectRef = UNiagaraFunctionLibrary::SpawnSystemAttached(
			ArrowData->OngoingEffect,
			AttachComponent,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			false
		);
	}
}

void AArrowBase::StopOngoingFeedback()
{
	if (IsValid(OngoingSoundRef))
	{
		OngoingSoundRef->Stop();
		OngoingSoundRef = nullptr;
	}

	if (IsValid(OngoingEffectRef))
	{
		OngoingEffectRef->Deactivate();
		OngoingEffectRef->DestroyComponent();
		OngoingEffectRef = nullptr;
	}
}

void AArrowBase::PlayEndFeedback(const FVector& FeedbackLocation)
{
	if (!IsValid(ArrowData))
	{
		return;
	}

	if (IsValid(ArrowData->EndSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ArrowData->EndSound,
			FeedbackLocation
		);
	}

	if (IsValid(ArrowData->EndEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ArrowData->EndEffect,
			FeedbackLocation,
			GetActorRotation()
		);
	}
}

void AArrowBase::HandleFlightExpired()
{
	if (!bIsInFlight || bHasImpacted)
	{
		return;
	}

	StopOngoingFeedback();
	SpawnPoolReturnEffect();
	ReturnToPool();
}

void AArrowBase::SpawnPoolReturnEffect()
{
	if (!IsValid(ArrowData) ||
		!IsValid(ArrowData->PoolReturnEffect))
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		ArrowData->PoolReturnEffect,
		GetActorLocation(),
		GetActorRotation()
	);
}

void AArrowBase::ScheduleFlightExpiry(const float Delay)
{
	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	if (Delay <= 0.0f)
	{
		HandleFlightExpired();
		return;
	}

	GetWorldTimerManager().SetTimer(
		RecycleTimerHandle,
		this,
		&AArrowBase::HandleFlightExpired,
		Delay,
		false
	);
}

void AArrowBase::ScheduleRecycle(const float Delay)
{
	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	if (Delay <= 0.0f)
	{
		ReturnToPool();
		return;
	}

	GetWorldTimerManager().SetTimer(
		RecycleTimerHandle,
		this,
		&AArrowBase::ReturnToPool,
		Delay,
		false
	);
}