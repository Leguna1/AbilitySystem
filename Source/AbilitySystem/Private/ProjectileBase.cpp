#include "ProjectileBase.h"

#include "CombatantComponent.h"
#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ImpactGroupSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "PayloadReceiver.h"
#include "ProjectileDataAsset.h"
#include "TargetableInterface.h"
#include "TimerManager.h"

namespace
{
	bool IsTargetDead(const AActor* Target)
	{
		const UCombatantComponent* Combatant = Target->FindComponentByClass<UCombatantComponent>();
		return IsValid(Combatant) && Combatant->IsDead();
	}

	FVector ResolveAimPoint(AActor* Target)
	{
		return Target->GetClass()->ImplementsInterface(UTargetableInterface::StaticClass())
			? ITargetableInterface::Execute_GetTargetAimLocation(Target)
			: Target->GetActorLocation();
	}
}

AProjectileBase::AProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	SetRootComponent(HitBox);

	HitBox->SetCollisionObjectType(ECC_WorldDynamic);
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	HitBox->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	HitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitBox->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(HitBox);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TipLocation = CreateDefaultSubobject<USceneComponent>(TEXT("TipLocation"));
	TipLocation->SetupAttachment(Mesh);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = HitBox;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f;
	ProjectileMovement->Velocity = FVector::ZeroVector;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bSweepCollision = true;
	ProjectileMovement->bAutoActivate = false;
}

void AProjectileBase::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(HitBox))
	{
		DefaultHitBoxExtent = HitBox->GetUnscaledBoxExtent();
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HitBox->OnComponentBeginOverlap.AddDynamic(this, &AProjectileBase::HandleHitBoxBeginOverlap);
	}

	if (IsValid(Mesh))
	{
		DefaultMeshRotation = Mesh->GetRelativeRotation();
	}

	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	ResetForPool();
}

void AProjectileBase::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsSpinning || !IsValid(Mesh))
	{
		return;
	}

	SpinElapsedTime += FMath::Max(DeltaTime, 0.0f);

	const float Alpha = SpinDuration > 0.0f
		? FMath::Clamp(SpinElapsedTime / SpinDuration, 0.0f, 1.0f)
		: 1.0f;

	FRotator NewRotation = SpinInitialRotation;
	NewRotation.Roll += FMath::Lerp(0.0f, SpinDegrees, Alpha);

	Mesh->SetRelativeRotation(NewRotation);

	if (Alpha >= 1.0f)
	{
		bIsSpinning = false;
		SetActorTickEnabled(false);
	}
}

float AProjectileBase::GetCalculatedDamage() const
{
	if (!IsValid(ProjectileData))
	{
		return 0.0f;
	}

	const float StrengthMultiplier = FMath::Lerp(1.0f, ProjectileData->MaximumDamageMultiplier, ShotParams.Strength);

	return ProjectileData->BaseDamage * StrengthMultiplier * ShotParams.DamageMultiplier;
}

void AProjectileBase::SpinBegin_Implementation()
{
	if (!IsValid(Mesh))
	{
		return;
	}

	SpinInitialRotation = Mesh->GetRelativeRotation();
	SpinElapsedTime = 0.0f;
	bIsSpinning = true;

	SetActorTickEnabled(true);
}

bool AProjectileBase::Fire_Implementation(const FVector& Direction, const FArrowShotParams& InShotParams)
{
	if (!IsValid(ProjectileData) || !IsValid(ProjectileMovement) || bIsInFlight)
	{
		return false;
	}

	++FlightSerial;

	const FVector NormalizedDirection = Direction.GetSafeNormal();

	if (NormalizedDirection.IsNearlyZero())
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);
	StopOngoingFeedback();

	ShotParams = InShotParams;
	ShotParams.Strength = FMath::Clamp(InShotParams.Strength, 0.0f, 1.0f);

	PiercesRemaining = FMath::Max(ShotParams.PierceCount, 0);
	CurrentPierceDamageFactor = 1.0f;
	HitActors.Reset();

	// Join before going live, so even a point-blank impact counts toward the group.
	JoinImpactGroup(ShotParams.ImpactGroup);

	bIsInFlight = true;
	bHasImpacted = false;

	const float Speed = FMath::Lerp(ProjectileData->MinimumSpeed, ProjectileData->MaximumSpeed, ShotParams.Strength);
	const float GravityScale = FMath::Lerp(ProjectileData->MaximumGravityScale, ProjectileData->MinimumGravityScale, ShotParams.Strength);

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

	if (ProjectileData->MaximumUntargetedTravelDistance > 0.0f)
	{
		ScheduleFlightExpiry(ProjectileData->MaximumUntargetedTravelDistance / Speed);
	}
	else
	{
		ScheduleFlightExpiry(ProjectileData->TargetedFlightLifespan);
	}

	if (ProjectileData->bSpinInFlight)
	{
		SpinDegrees = ProjectileData->SpinDegrees;
		SpinDuration = ProjectileData->SpinDuration;
		SpinBegin();
	}

	StartOngoingFeedback();

	return true;
}

bool AProjectileBase::ActivateFromPool(UProjectileDataAsset* NewProjectileData)
{
	if (!IsValid(NewProjectileData))
	{
		return false;
	}

	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);
	StopOngoingFeedback();

	if (GetAttachParentActor() != nullptr ||
		(IsValid(GetRootComponent()) && GetRootComponent()->GetAttachParent() != nullptr))
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	ProjectileData = NewProjectileData;

	Velocity = FVector::ZeroVector;

	bIsInFlight = false;
	bHasImpacted = false;

	bIsSpinning = false;
	SpinElapsedTime = 0.0f;

	ShotParams = FArrowShotParams();
	PiercesRemaining = 0;
	CurrentPierceDamageFactor = 1.0f;
	HitActors.Reset();

	if (IsValid(ProjectileMovement))
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->ProjectileGravityScale = 0.0f;
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->HomingTargetComponent = nullptr;
	}

	if (IsValid(HitBox))
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		// Several projectile kinds can share one class, so the size comes from the data each time.
		const FVector Extent = ProjectileData->HitBoxExtent.IsNearlyZero()
			? DefaultHitBoxExtent
			: ProjectileData->HitBoxExtent;

		HitBox->SetBoxExtent(Extent, false);
	}

	if (IsValid(Mesh))
	{
		// No mesh is fine: effect-only projectiles are just their Ongoing Effect.
		Mesh->SetStaticMesh(ProjectileData->Mesh);
		Mesh->SetRelativeRotation(DefaultMeshRotation);
	}

	SetActorTickEnabled(false);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	return true;
}

void AProjectileBase::ResetForPool()
{
	LeaveImpactGroup();
	ReleaseStuckActor();
	RedirectTargetActor.Reset();

	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);
	GetWorldTimerManager().ClearTimer(RedirectTimerHandle);

	StopOngoingFeedback();

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
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->HomingTargetComponent = nullptr;
	}

	if (IsValid(HitBox))
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (IsValid(Mesh))
	{
		Mesh->SetRelativeRotation(DefaultMeshRotation);
	}

	ProjectileData = nullptr;

	Velocity = FVector::ZeroVector;

	bIsInFlight = false;
	bHasImpacted = false;

	bIsSpinning = false;
	SpinElapsedTime = 0.0f;

	ShotParams = FArrowShotParams();
	PiercesRemaining = 0;
	CurrentPierceDamageFactor = 1.0f;
	HitActors.Reset();

	// Pooled projectiles are shared: the next user sets its own owner and instigator.
	SetOwner(nullptr);
	SetInstigator(nullptr);

	SetActorTickEnabled(false);
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AProjectileBase::ReturnToPool()
{
	ResetForPool();
	OnReadyToRecycle.Broadcast(this);
}

bool AProjectileBase::Redirect(const FVector& NewDirection)
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

bool AProjectileBase::IsInFlight() const
{
	return bIsInFlight &&
		!bHasImpacted &&
		IsValid(ProjectileMovement) &&
		ProjectileMovement->IsActive();
}

bool AProjectileBase::SetRemainingFlightTime(const float Duration)
{
	if (!IsInFlight())
	{
		return false;
	}

	ScheduleFlightExpiry(Duration);
	return true;
}

void AProjectileBase::HandleHitBoxBeginOverlap(
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

	// Projectiles pass through each other.
	if (OtherActor->IsA<AProjectileBase>())
	{
		return;
	}

	UPrimitiveComponent* HitComponent = SweepResult.GetComponent();

	if (!IsValid(HitComponent) || HitComponent->GetOwner() == this)
	{
		HitComponent = OtherComponent;
	}

	if (!IsValid(HitComponent) || HitComponent->GetOwner() == this)
	{
		return;
	}

	if (HitActors.Contains(OtherActor))
	{
		return;
	}

	HandleImpact(OtherActor, HitComponent, bFromSweep, SweepResult);
}

void AProjectileBase::HandleImpact(
	AActor* HitActor,
	UPrimitiveComponent* HitComponent,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bHasImpacted ||
		!IsValid(HitActor) ||
		!IsValid(ProjectileData) ||
		HitActors.Contains(HitActor))
	{
		return;
	}

	// Recorded before delivery, so nothing during the hit (death, ragdoll) can hit it twice.
	HitActors.Add(HitActor);

	FVector ImpactLocation = GetActorLocation();

	if (bFromSweep && !SweepResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(SweepResult.ImpactPoint);
	}

	// Each target passed through lowers the damage for the next one.
	const float ImpactDamage = GetCalculatedDamage() * CurrentPierceDamageFactor;

	FAbilityPayload Payload;
	Payload.Damage = ImpactDamage;
	Payload.Instigator = GetInstigator();
	Payload.Causer = this;
	Payload.Hit = SweepResult;
	ShotParams.Hit.ApplyTo(Payload);

	// Deliver first: acceptance decides hit vs miss feedback, and whether the projectile can pierce.
	const bool bPayloadAccepted = UCombatantComponent::DeliverPayload(HitActor, Payload);

	PlayImpactFeedback(bPayloadAccepted, ImpactLocation, HitComponent, SweepResult);

	// Pierce: a living target that took the hit is passed through while pierces remain.
	const bool bPierce =
		bPayloadAccepted &&
		PiercesRemaining > 0 &&
		IsValid(HitActor->FindComponentByClass<UCombatantComponent>());

	if (bPierce)
	{
		--PiercesRemaining;
		CurrentPierceDamageFactor *= ShotParams.PierceDamageFactor;

		OnProjectileHit.Broadcast(this, HitActor, ImpactDamage, SweepResult);
		return;
	}

	// Final impact: stop here.
	bHasImpacted = true;
	bIsInFlight = false;
	bIsSpinning = false;

	const bool bStick = ProjectileData->bStickOnImpact;
	const float ImpulseMultiplier = ProjectileData->ImpactImpulseMultiplier;
	const float StuckLifespan = ProjectileData->ImpactLifespan;

	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);
	StopOngoingFeedback();
	LeaveImpactGroup();

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

	OnProjectileHit.Broadcast(this, HitActor, ImpactDamage, SweepResult);

	if (IsValid(HitComponent) && HitComponent->IsSimulatingPhysics())
	{
		HitComponent->AddImpulseAtLocation(Velocity * ImpulseMultiplier, ImpactLocation);
	}

	// Bolts and orbs are done at their first stop; so is anything whose target the hit removed.
	if (!bStick || !IsValid(HitActor) || !IsValid(HitComponent))
	{
		ReturnToPool();
		return;
	}

	const FAttachmentTransformRules AttachmentRules(
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		EAttachmentRule::KeepWorld,
		true);

	AttachToComponent(HitComponent, AttachmentRules);

	// If the target is destroyed later (corpse cleanup), go back to the pool instead of floating.
	StuckToActor = HitActor;
	HitActor->OnDestroyed.AddUniqueDynamic(this, &AProjectileBase::HandleStuckActorDestroyed);

	ScheduleRecycle(StuckLifespan);
}

void AProjectileBase::StartOngoingFeedback()
{
	if (!IsValid(ProjectileData))
	{
		return;
	}

	USceneComponent* AttachComponent = IsValid(TipLocation) ? TipLocation.Get() : GetRootComponent();

	if (!IsValid(AttachComponent))
	{
		return;
	}

	if (IsValid(ProjectileData->OngoingSound))
	{
		OngoingSoundRef = UGameplayStatics::SpawnSoundAttached(ProjectileData->OngoingSound, AttachComponent);
	}

	if (IsValid(ProjectileData->OngoingEffect))
	{
		OngoingEffectRef = UNiagaraFunctionLibrary::SpawnSystemAttached(
			ProjectileData->OngoingEffect,
			AttachComponent,
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			false);
	}
}

void AProjectileBase::StopOngoingFeedback()
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

void AProjectileBase::HandleFlightExpired()
{
	if (!bIsInFlight || bHasImpacted)
	{
		return;
	}

	StopOngoingFeedback();
	SpawnPoolReturnEffect();
	ReturnToPool();
}

void AProjectileBase::SpawnPoolReturnEffect()
{
	if (!IsValid(ProjectileData) || !IsValid(ProjectileData->PoolReturnEffect))
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ProjectileData->PoolReturnEffect, GetActorLocation(), GetActorRotation());
}

void AProjectileBase::ScheduleFlightExpiry(const float Delay)
{
	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	if (Delay <= 0.0f)
	{
		HandleFlightExpired();
		return;
	}

	GetWorldTimerManager().SetTimer(RecycleTimerHandle, this, &AProjectileBase::HandleFlightExpired, Delay, false);
}

void AProjectileBase::ScheduleRecycle(const float Delay)
{
	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	if (Delay <= 0.0f)
	{
		ReturnToPool();
		return;
	}

	GetWorldTimerManager().SetTimer(RecycleTimerHandle, this, &AProjectileBase::ReturnToPool, Delay, false);
}

void AProjectileBase::JoinImpactGroup(const FImpactGroupHandle& InImpactGroup)
{
	LeaveImpactGroup();

	UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this);

	if (InImpactGroup.IsValid() &&
		IsValid(ImpactGroups) &&
		ImpactGroups->AddMember(InImpactGroup))
	{
		ImpactGroup = InImpactGroup;
	}
}

void AProjectileBase::LeaveImpactGroup()
{
	if (!ImpactGroup.IsValid())
	{
		return;
	}

	if (UImpactGroupSubsystem* ImpactGroups = UImpactGroupSubsystem::Get(this))
	{
		ImpactGroups->ResolveMember(ImpactGroup);
	}

	ImpactGroup = FImpactGroupHandle();
}

void AProjectileBase::PlayImpactFeedback(const bool bHitTarget, const FVector& ImpactLocation, const UPrimitiveComponent* HitComponent, const FHitResult& Hit) const
{
	UImpactGroupSubsystem* ImpactFeedback = UImpactGroupSubsystem::Get(this);

	if (!IsValid(ImpactFeedback) || !IsValid(ProjectileData))
	{
		return;
	}

	FImpactReport Report;
	Report.Group = ImpactGroup;
	Report.Result = bHitTarget ? EImpactResult::Hit : EImpactResult::Miss;
	Report.Location = ImpactLocation;
	Report.Rotation = GetActorRotation();

	Report.OwnHitFeedback.Sound = ProjectileData->EndSound;
	Report.OwnHitFeedback.Effect = ProjectileData->EndEffect;
	Report.OwnHitFeedback.PitchMin = ProjectileData->EndSoundPitchMin;
	Report.OwnHitFeedback.PitchMax = ProjectileData->EndSoundPitchMax;

	Report.Hit = Hit;
	Report.HitComponent = HitComponent;

	ImpactFeedback->PlayImpact(Report);
}

void AProjectileBase::ScheduleRedirect(const FVector& TargetPoint, const float Delay, const bool bDisableGravity)
{
	ScheduleRedirectToActor(nullptr, TargetPoint, Delay, bDisableGravity, 0.0f);
}

void AProjectileBase::ScheduleRedirectToActor(AActor* TargetActor, const FVector& FallbackPoint, const float Delay, const bool bDisableGravity, const float HomingAcceleration)
{
	GetWorldTimerManager().ClearTimer(RedirectTimerHandle);

	RedirectTargetActor = TargetActor;
	RedirectTargetPoint = FallbackPoint;
	bRedirectDisablesGravity = bDisableGravity;
	RedirectHomingAcceleration = FMath::Max(HomingAcceleration, 0.0f);

	if (Delay <= 0.0f)
	{
		HandleScheduledRedirect();
		return;
	}

	GetWorldTimerManager().SetTimer(RedirectTimerHandle, this, &AProjectileBase::HandleScheduledRedirect, Delay, false);
}

void AProjectileBase::HandleScheduledRedirect()
{
	if (!IsInFlight())
	{
		return;
	}

	if (bRedirectDisablesGravity && IsValid(ProjectileMovement))
	{
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}

	AActor* TargetActor = RedirectTargetActor.Get();
	const bool bTargetAlive = IsValid(TargetActor) && !IsTargetDead(TargetActor);

	// A living target is aimed at where it is now; otherwise the projectile keeps its landing point.
	const FVector AimPoint = bTargetAlive ? ResolveAimPoint(TargetActor) : RedirectTargetPoint;
	Redirect((AimPoint - GetActorLocation()).GetSafeNormal());

	if (bTargetAlive && RedirectHomingAcceleration > 0.0f && IsValid(ProjectileMovement))
	{
		ProjectileMovement->HomingTargetComponent = TargetActor->GetRootComponent();
		ProjectileMovement->HomingAccelerationMagnitude = RedirectHomingAcceleration;
		ProjectileMovement->bIsHomingProjectile = true;
	}
}

void AProjectileBase::HandleStuckActorDestroyed(AActor* DestroyedActor)
{
	ReturnToPool();
}

void AProjectileBase::ReleaseStuckActor()
{
	if (AActor* StuckActor = StuckToActor.Get())
	{
		StuckActor->OnDestroyed.RemoveDynamic(this, &AProjectileBase::HandleStuckActorDestroyed);
	}

	StuckToActor.Reset();
}
