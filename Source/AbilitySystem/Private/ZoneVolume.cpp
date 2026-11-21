#include "ZoneVolume.h"

#include "AdventureComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "ZoneVolume"

AZoneVolume::AZoneVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Area = CreateDefaultSubobject<UBoxComponent>(TEXT("Area"));
	SetRootComponent(Area);
	Area->SetBoxExtent(FVector(1000.0f, 1000.0f, 500.0f));
	Area->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Area->SetCollisionObjectType(ECC_WorldDynamic);
	Area->SetCollisionResponseToAllChannels(ECR_Ignore);
	Area->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Area->SetGenerateOverlapEvents(true);
	Area->SetCanEverAffectNavigation(false);
	Area->ShapeColor = FColor::Cyan;

	ZoneName = LOCTEXT("DefaultName", "Unnamed Lands");
}

void AZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	Area->OnComponentBeginOverlap.AddDynamic(this, &AZoneVolume::HandleBeginOverlap);

	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->OnPlayerReady.AddUniqueDynamic(this, &AZoneVolume::HandlePlayerReady);
	}
}

void AZoneVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this))
	{
		Progress->OnPlayerReady.RemoveDynamic(this, &AZoneVolume::HandlePlayerReady);
	}

	Super::EndPlay(EndPlayReason);
}

void AZoneVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* EnteringPawn = Cast<APawn>(OtherActor);

	if (IsValid(EnteringPawn) && EnteringPawn->IsPlayerControlled())
	{
		Enter(EnteringPawn);
	}
}

void AZoneVolume::HandlePlayerReady(APawn* PlayerPawn)
{
	if (IsValid(PlayerPawn) && Area->IsOverlappingActor(PlayerPawn))
	{
		Enter(PlayerPawn);
	}
}

void AZoneVolume::Enter(APawn* PlayerPawn)
{
	UWorldProgressSubsystem* Progress = UWorldProgressSubsystem::Get(this);
	UAdventureComponent* Adventure = UAdventureComponent::Get(this);

	// Before the player is set up (the first moments of a session) nothing is announced; Handle Player Ready catches up.
	if (!IsValid(Progress) || !IsValid(Adventure) || !Adventure->IsPlayerReady())
	{
		return;
	}

	const bool bFirstTime = !ZoneId.IsNone() && !Progress->HasFlag(ZoneId);

	if (bFirstTime)
	{
		Progress->SetFlag(ZoneId);
		Adventure->ShowBanner(ZoneName, Subtitle.IsEmpty() ? LOCTEXT("Discovered", "Discovered") : Subtitle, 5.0f);
		Adventure->GiveExperience(DiscoveryExperience);
	}
	else
	{
		Adventure->ShowBanner(ZoneName, Subtitle, 3.0f);
	}

	OnPlayerEntered(PlayerPawn, bFirstTime);
}

#undef LOCTEXT_NAMESPACE
