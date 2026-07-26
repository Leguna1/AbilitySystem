#include "ArrowBase.h"

#include "Camera/CameraComponent.h"
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
		HitBox->OnComponentBeginOverlap.AddDynamic(this, &AArrowBase::HandleHitBoxBeginOverlap);
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

void AArrowBase::SetArrowStats(const FArrowStats& NewArrowStats)
{
	ArrowStats = NewArrowStats;
}

float AArrowBase::GetCalculatedDamage() const
{
	const float StrengthMultiplier = FMath::Lerp(
		1.0f,
		ArrowStats.MaximumDamageMultiplier,
		FiredStrength
	);

	return ArrowStats.BaseDamage * StrengthMultiplier;
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

bool AArrowBase::Fire_Implementation(const FVector& Direction, const float Strength)
{
	if (!IsValid(ProjectileMovement) || bIsInFlight)
	{
		return false;
	}

	const FVector NormalizedDirection = Direction.GetSafeNormal();

	if (NormalizedDirection.IsNearlyZero())
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);
	GetWorldTimerManager().ClearTimer(TrailDestroyTimerHandle);

	DestroyTrailEffect();

	FiredStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
	bIsInFlight = true;
	bHasImpacted = false;

	const float Speed = FMath::Lerp(
		ArrowConfiguration.MinSpeed,
		ArrowConfiguration.MaxSpeed,
		FiredStrength
	) * ArrowStats.SpeedMultiplier;

	const float GravityScale = FMath::Lerp(
		ArrowConfiguration.MaxGravity,
		ArrowConfiguration.MinGravity,
		FiredStrength
	) * ArrowStats.GravityMultiplier;

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

	ScheduleRecycle(ArrowConfiguration.FlyingLifespan);
	SpinBegin();

	USoundBase* WhooshSound = IsValid(ArrowStats.WhooshSound)
		? ArrowStats.WhooshSound
		: ArrowConfiguration.WhooshSound;

	UNiagaraSystem* TrailEffect = IsValid(ArrowStats.TrailEffect)
		? ArrowStats.TrailEffect
		: ArrowConfiguration.TrailEffect;

	if (IsValid(WhooshSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			WhooshSound,
			GetActorLocation()
		);
	}

	if (IsValid(TrailEffect) && IsValid(TipLocation))
	{
		TrailEffectRef = UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailEffect,
			TipLocation,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false
		);
	}

	return true;
}

void AArrowBase::ActivateFromPool(const FArrowStats& NewArrowStats)
{
	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);
	GetWorldTimerManager().ClearTimer(TrailDestroyTimerHandle);

	DestroyTrailEffect();

	if (GetAttachParentActor() != nullptr || GetRootComponent()->GetAttachParent() != nullptr)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	ArrowStats = NewArrowStats;
	FiredStrength = 0.0f;
	Velocity = FVector::ZeroVector;

	bIsInFlight = false;
	bHasImpacted = false;
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
		ArrowMesh->SetRelativeRotation(DefaultArrowMeshRotation);
	}

	SetActorTickEnabled(false);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
}

void AArrowBase::ResetForPool()
{
	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);
	GetWorldTimerManager().ClearTimer(TrailDestroyTimerHandle);

	DestroyTrailEffect();

	if (GetAttachParentActor() != nullptr ||
		(IsValid(GetRootComponent()) && GetRootComponent()->GetAttachParent() != nullptr))
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

	ArrowStats = FArrowStats();
	FiredStrength = 0.0f;
	Velocity = FVector::ZeroVector;

	bIsInFlight = false;
	bHasImpacted = false;
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

	HandleImpact(OtherActor, HitComponent, bFromSweep, SweepResult);
}

void AArrowBase::HandleImpact(
	AActor* HitActor,
	UPrimitiveComponent* HitComponent,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bHasImpacted || !IsValid(HitActor))
	{
		return;
	}

	bHasImpacted = true;
	bIsInFlight = false;
	bIsSpinning = false;

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

		AttachToComponent(HitComponent, AttachmentRules);
	}

	USoundBase* ImpactSound = IsValid(ArrowStats.ImpactSound)
		? ArrowStats.ImpactSound
		: ArrowConfiguration.ArrowImpactSound;

	UNiagaraSystem* ImpactEffect = IsValid(ArrowStats.ImpactEffect)
		? ArrowStats.ImpactEffect
		: ArrowConfiguration.ImpactEffect;

	const FVector ImpactLocation = bFromSweep && !SweepResult.ImpactPoint.IsNearlyZero()
	? FVector(SweepResult.ImpactPoint)
	: GetActorLocation();

	if (IsValid(ImpactSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ImpactSound,
			ImpactLocation
		);
	}

	if (IsValid(ImpactEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ImpactEffect,
			ImpactLocation
		);
	}

	if (IsValid(HitComponent) && HitComponent->IsSimulatingPhysics())
	{
		HitComponent->AddImpulseAtLocation(
			Velocity * ArrowStats.ImpactImpulseMultiplier,
			ImpactLocation
		);
	}

	OnArrowHit.Broadcast(
		this,
		HitActor,
		GetCalculatedDamage(),
		SweepResult
	);

	ScheduleRecycle(ArrowConfiguration.ImpactLifespan);

	GetWorldTimerManager().SetTimer(
		TrailDestroyTimerHandle,
		this,
		&AArrowBase::DestroyTrailEffect,
		0.2f,
		false
	);
}

void AArrowBase::DestroyTrailEffect()
{
	if (!IsValid(TrailEffectRef))
	{
		return;
	}

	TrailEffectRef->Deactivate();
	TrailEffectRef->DestroyComponent();
	TrailEffectRef = nullptr;
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