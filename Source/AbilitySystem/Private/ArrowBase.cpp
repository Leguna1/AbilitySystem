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
#include "PayloadReceiver.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "ImpactGroupSubsystem.h"
#include "CombatantComponent.h"

AArrowBase::AArrowBase()
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
	ProjectileMovement->bSweepCollision = true;
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
		ShotParams.Strength
	);

	return ArrowData->BaseDamage * StrengthMultiplier * ShotParams.DamageMultiplier;
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

bool AArrowBase::Fire_Implementation(const FVector& Direction, const FArrowShotParams& InShotParams)
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

	ShotParams = InShotParams;
	ShotParams.Strength = FMath::Clamp(InShotParams.Strength, 0.0f, 1.0f);

	// Join before the arrow goes live, so even a point-blank impact counts toward the group.
	JoinImpactGroup(ShotParams.ImpactGroup);
	
	bIsInFlight = true;
	bHasImpacted = false;

	const float Speed = FMath::Lerp(
		ArrowData->MinimumSpeed,
		ArrowData->MaximumSpeed,
		ShotParams.Strength
	);

	const float GravityScale = FMath::Lerp(
		ArrowData->MaximumGravityScale,
		ArrowData->MinimumGravityScale,
		ShotParams.Strength
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

	if (ArrowData->MaximumUntargetedTravelDistance > 0.0f)
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
	
	Velocity = FVector::ZeroVector;

	bIsInFlight = false;
	bHasImpacted = false;
	
	bIsSpinning = false;
	SpinElapsedTime = 0.0f;
	
	ShotParams = FArrowShotParams();

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
	LeaveImpactGroup();
	GetWorldTimerManager().ClearTimer(RecycleTimerHandle);

	GetWorldTimerManager().ClearTimer(RedirectTimerHandle);
	
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
	
	Velocity = FVector::ZeroVector;

	bIsInFlight = false;
	bHasImpacted = false;
	
	bIsSpinning = false;
	SpinElapsedTime = 0.0f;
	ShotParams = FArrowShotParams();
	
	
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

bool AArrowBase::SetRemainingFlightTime(const float Duration)
{
	if (!IsInFlight())
	{
		return false;
	}

	ScheduleFlightExpiry(Duration);
	return true;
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

	if (OtherActor->IsA<AArrowBase>())
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

		AttachToComponent(HitComponent, AttachmentRules);
	}

	FVector ImpactLocation = GetActorLocation();

	if (bFromSweep && !SweepResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(SweepResult.ImpactPoint);
	}

	const float ImpactDamage = GetCalculatedDamage();

	// Deliver first: whether the target accepted the payload decides hit vs miss feedback.
	FAbilityPayload Payload;
	Payload.Damage = ImpactDamage;
	Payload.Instigator = GetInstigator();
	Payload.Causer = this;
	Payload.Hit = SweepResult;
	Payload.SourceAbilityTags = ShotParams.SourceAbilityTags;
	Payload.Statuses = ShotParams.Statuses;

	const bool bPayloadAccepted = UCombatantComponent::DeliverPayload(HitActor, Payload);

	PlayImpactFeedback(bPayloadAccepted, ImpactLocation, HitComponent, SweepResult);
	LeaveImpactGroup();

	if (IsValid(HitComponent) &&
		HitComponent->IsSimulatingPhysics())
	{
		HitComponent->AddImpulseAtLocation(
			Velocity * ArrowData->ImpactImpulseMultiplier,
			ImpactLocation
		);
	}

	// Observers (abilities tracking hits, combo systems) hear about every impact.
	OnArrowHit.Broadcast(
		this,
		HitActor,
		ImpactDamage,
		SweepResult
	);

	ScheduleRecycle(ArrowData->ImpactLifespan);
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
void AArrowBase::JoinImpactGroup(const FImpactGroupHandle& InImpactGroup)
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

void AArrowBase::LeaveImpactGroup()
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


void AArrowBase::PlayImpactFeedback(const bool bHitTarget, const FVector& ImpactLocation, const UPrimitiveComponent* HitComponent, const FHitResult& Hit) const
{
	UImpactGroupSubsystem* ImpactFeedback = UImpactGroupSubsystem::Get(this);

	if (!IsValid(ImpactFeedback) || !IsValid(ArrowData))
	{
		return;
	}

	FImpactReport Report;
	Report.Group = ImpactGroup;
	Report.Result = bHitTarget ? EImpactResult::Hit : EImpactResult::Miss;
	Report.Location = ImpactLocation;
	Report.Rotation = GetActorRotation();

	Report.OwnHitFeedback.Sound = ArrowData->EndSound;
	Report.OwnHitFeedback.Effect = ArrowData->EndEffect;
	Report.OwnHitFeedback.PitchMin = ArrowData->EndSoundPitchMin;
	Report.OwnHitFeedback.PitchMax = ArrowData->EndSoundPitchMax;

	Report.Hit = Hit;
	Report.HitComponent = HitComponent;

	ImpactFeedback->PlayImpact(Report);
}
void AArrowBase::ScheduleRedirect(const FVector& TargetPoint, const float Delay, const bool bDisableGravity)
{
	GetWorldTimerManager().ClearTimer(RedirectTimerHandle);

	RedirectTargetPoint = TargetPoint;
	bRedirectDisablesGravity = bDisableGravity;

	if (Delay <= 0.0f)
	{
		HandleScheduledRedirect();
		return;
	}

	GetWorldTimerManager().SetTimer(
		RedirectTimerHandle,
		this,
		&AArrowBase::HandleScheduledRedirect,
		Delay,
		false
	);
}

void AArrowBase::HandleScheduledRedirect()
{
	if (!IsInFlight())
	{
		return;
	}

	if (bRedirectDisablesGravity && IsValid(ProjectileMovement))
	{
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}

	Redirect((RedirectTargetPoint - GetActorLocation()).GetSafeNormal());
}