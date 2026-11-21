#include "PuzzleSwitch.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "ProgressGate.h"
#include "TimerManager.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "PuzzleSwitch"

APuzzleSwitch::APuzzleSwitch()
{
	SetPlaceholder(EAdventurePropShape::Cylinder, FVector(0.5f, 0.5f, 1.0f), 150.0f);
	DisplayName = LOCTEXT("DefaultName", "Switch");

	// Arrows overlap the mesh to hit it (Hit switches).
	Mesh->SetGenerateOverlapEvents(true);

	StepArea = CreateDefaultSubobject<UBoxComponent>(TEXT("StepArea"));
	StepArea->SetupAttachment(SceneRoot);
	StepArea->SetBoxExtent(FVector(70.0f, 70.0f, 60.0f));
	StepArea->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	StepArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StepArea->SetCollisionObjectType(ECC_WorldDynamic);
	StepArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	StepArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	StepArea->SetGenerateOverlapEvents(true);
}

void APuzzleSwitch::BeginPlay()
{
	Super::BeginPlay();

	if (Activation == ESwitchActivation::Step)
	{
		StepArea->OnComponentBeginOverlap.AddDynamic(this, &APuzzleSwitch::HandleStepBegin);
		StepArea->OnComponentEndOverlap.AddDynamic(this, &APuzzleSwitch::HandleStepEnd);

		// A plate is walked over, not bumped into: flat on the ground and no obstacle.
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetRelativeScale3D(FVector(1.4f, 1.4f, 0.08f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
	}
	else
	{
		StepArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SetStateColor(OffColor);
}

void APuzzleSwitch::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ResetTimer);
	Super::EndPlay(EndPlayReason);
}

void APuzzleSwitch::SetOn(const bool bNewOn)
{
	if (bOn == bNewOn)
	{
		return;
	}

	bOn = bNewOn;
	GetWorldTimerManager().ClearTimer(ResetTimer);

	for (AProgressGate* Gate : Targets)
	{
		if (!IsValid(Gate))
		{
			continue;
		}

		if (bOn)
		{
			Gate->OpenGate(true);
		}
		else
		{
			Gate->CloseGate();
		}
	}

	if (bOn)
	{
		if (UWorldProgressSubsystem* Progress = GetProgress())
		{
			Progress->SetFlag(FlagToSet);
		}

		if (ResetAfter > 0.0f)
		{
			GetWorldTimerManager().SetTimer(ResetTimer, this, &APuzzleSwitch::TurnOffByTimer, ResetAfter, false);
		}
	}

	SetStateColor(bOn ? OnColor : OffColor);
	OnSwitchChanged(bOn);
}

void APuzzleSwitch::Toggle()
{
	if (!bOn)
	{
		SetOn(true);
	}
	else if (bCanTurnOff)
	{
		SetOn(false);
	}
}

void APuzzleSwitch::TurnOffByTimer()
{
	SetOn(false);
}

/* -------------------- Lever -------------------- */

FText APuzzleSwitch::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	if (Activation != ESwitchActivation::Interact || (bOn && !bCanTurnOff))
	{
		return FText::GetEmpty();
	}

	return FText::Format(LOCTEXT("Use", "Use {0}"), DisplayName);
}

bool APuzzleSwitch::CanInteract_Implementation(AActor* Interactor) const
{
	return Activation == ESwitchActivation::Interact && (!bOn || bCanTurnOff);
}

void APuzzleSwitch::Interact_Implementation(AActor* Interactor)
{
	if (CanInteract_Implementation(Interactor))
	{
		Toggle();
	}
}

/* -------------------- Crystal -------------------- */

bool APuzzleSwitch::ReceivePayload_Implementation(const FAbilityPayload& Payload)
{
	if (Activation != ESwitchActivation::Hit)
	{
		return false;
	}

	Toggle();
	return true;
}

/* -------------------- Pressure plate -------------------- */

void APuzzleSwitch::HandleStepBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<APawn>(OtherActor))
	{
		SetOn(true);
	}
}

void APuzzleSwitch::HandleStepEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!bHoldToStayOn || !Cast<APawn>(OtherActor))
	{
		return;
	}

	TArray<AActor*> StillStanding;
	StepArea->GetOverlappingActors(StillStanding, APawn::StaticClass());

	if (StillStanding.IsEmpty())
	{
		SetOn(false);
	}
}

#undef LOCTEXT_NAMESPACE
