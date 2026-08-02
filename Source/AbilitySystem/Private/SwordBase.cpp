// SwordBase.cpp
#include "SwordBase.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

ASwordBase::ASwordBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// Neutral root so the mesh and hitbox are positioned independently. If the
	// hitbox were a child of the mesh, an offset mesh pivot (grip vs. blade)
	// would drag the hitbox out of place.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	SwordMesh->SetupAttachment(Root);
	SwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(Root);
	HitBox->SetBoxExtent(FVector(5.f, 5.f, 50.f));
	// Off by default -- queried only during an active swing.
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitBox->SetCollisionResponseToAllChannels(ECR_Overlap);
	HitBox->SetGenerateOverlapEvents(false);
}

void ASwordBase::BeginPlay()
{
	Super::BeginPlay();

	HitBox->OnComponentBeginOverlap.AddDynamic(this, &ASwordBase::HandleHitBoxOverlap);
}

void ASwordBase::BeginHitDetection()
{
	HitActorsThisSwing.Reset();
	bHitDetectionActive = true;

	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetGenerateOverlapEvents(true);

	// Catch actors already inside the box at swing start (overlap-begin only
	// fires on entry, so a target already touching wouldn't otherwise register).
	TArray<AActor*> Overlapping;
	HitBox->GetOverlappingActors(Overlapping);
	for (AActor* Actor : Overlapping)
	{
		if (IsValid(Actor) && Actor != GetOwner() && !HitActorsThisSwing.Contains(Actor))
		{
			HitActorsThisSwing.Add(Actor);
			OnSwordHit.Broadcast(Actor, FHitResult());
		}
	}
}

void ASwordBase::EndHitDetection()
{
	bHitDetectionActive = false;

	HitBox->SetGenerateOverlapEvents(false);
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HitActorsThisSwing.Reset();
}

void ASwordBase::HandleHitBoxOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bHitDetectionActive || !IsValid(OtherActor))
	{
		return;
	}

	// Never hit the wielder.
	if (OtherActor == GetOwner())
	{
		return;
	}

	// One hit per actor per swing.
	if (HitActorsThisSwing.Contains(OtherActor))
	{
		return;
	}

	HitActorsThisSwing.Add(OtherActor);
	OnSwordHit.Broadcast(OtherActor, SweepResult);
}