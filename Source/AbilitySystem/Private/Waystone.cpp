#include "Waystone.h"

#include "AdventureComponent.h"
#include "Components/ArrowComponent.h"
#include "EngineUtils.h"
#include "WorldProgressSubsystem.h"

#define LOCTEXT_NAMESPACE "Waystone"

AWaystone::AWaystone()
{
	SetPlaceholder(EAdventurePropShape::Cone, FVector(0.8f, 0.8f, 2.4f), 290.0f);
	DisplayName = LOCTEXT("DefaultName", "Waystone");

	ArrivalPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("ArrivalPoint"));
	ArrivalPoint->SetupAttachment(SceneRoot);

	// In front of the stone and a capsule's half height above the ground.
	ArrivalPoint->SetRelativeLocation(FVector(200.0f, 0.0f, 100.0f));
}

void AWaystone::BeginPlay()
{
	Super::BeginPlay();

	UWorldProgressSubsystem* Progress = GetProgress();

	if (bStartsAttuned && IsValid(Progress))
	{
		Progress->SetFlag(WaystoneId);
	}

	const bool bAttuned = IsAttuned();

	SetStateColor(bAttuned ? FLinearColor(0.3f, 0.8f, 1.0f) : FLinearColor(0.5f, 0.5f, 0.55f));
	SetGlowEnabled(bAttuned);

	if (bAttuned)
	{
		OnAttuned(true);
	}
}

bool AWaystone::IsAttuned() const
{
	const UWorldProgressSubsystem* Progress = GetProgress();
	return IsValid(Progress) && Progress->HasFlag(WaystoneId);
}

FTransform AWaystone::GetArrivalTransform() const
{
	return IsValid(ArrivalPoint) ? ArrivalPoint->GetComponentTransform() : GetActorTransform();
}

FText AWaystone::GetInteractionPrompt_Implementation(AActor* Interactor) const
{
	return IsAttuned()
		? FText::Format(LOCTEXT("Use", "Rest at {0}"), DisplayName)
		: FText::Format(LOCTEXT("Attune", "Attune to {0}"), DisplayName);
}

void AWaystone::Interact_Implementation(AActor* Interactor)
{
	UAdventureComponent* Adventure = GetAdventure();
	UWorldProgressSubsystem* Progress = GetProgress();

	if (!IsValid(Adventure) || !IsValid(Progress))
	{
		return;
	}

	if (!IsAttuned())
	{
		Progress->SetFlag(WaystoneId);
		SetStateColor(FLinearColor(0.3f, 0.8f, 1.0f));
		SetGlowEnabled(true);
		OnAttuned(false);

		Adventure->ShowBanner(FText::Format(LOCTEXT("AttunedBanner", "{0} attuned"), DisplayName),
			LOCTEXT("AttunedSubtitle", "You will wake here if you fall."), 4.0f);
	}

	// Using a waystone always makes it the place to wake up, and always saves.
	Rest();
	ShowMenu();
}

void AWaystone::Rest()
{
	if (UAdventureComponent* Adventure = GetAdventure())
	{
		Adventure->SetCheckpoint(GetArrivalTransform());
		Adventure->RestorePlayer();
		Adventure->RequestSave();
	}
}

void AWaystone::ShowMenu()
{
	UAdventureComponent* Adventure = GetAdventure();

	if (!IsValid(Adventure))
	{
		return;
	}

	TArray<FAdventurePanelOption> Options;
	TravelDestinations.Reset();

	for (TActorIterator<AWaystone> It(GetWorld()); It; ++It)
	{
		AWaystone* Other = *It;

		if (IsValid(Other) && Other != this && Other->IsAttuned())
		{
			TravelDestinations.Add(Other);
			Options.Add(FAdventurePanelOption(FText::Format(LOCTEXT("TravelTo", "Travel to {0}"), Other->GetDisplayName())));
		}
	}

	Options.Add(FAdventurePanelOption(LOCTEXT("Leave", "Leave")));

	const FText Body = TravelDestinations.IsEmpty()
		? LOCTEXT("BodyAlone", "You rest. Your wounds close and the way back here is fixed in your mind.\n\nAttune other waystones to travel between them.")
		: LOCTEXT("BodyTravel", "You rest. Your wounds close and the way back here is fixed in your mind.\n\nThe stone hums with the echo of others you have touched.");

	Adventure->OpenPanel(DisplayName, Body, Options, FAdventurePanelChoiceDelegate::CreateUObject(this, &AWaystone::HandleMenuChoice));
}

void AWaystone::HandleMenuChoice(const int32 OptionIndex)
{
	UAdventureComponent* Adventure = GetAdventure();

	if (!IsValid(Adventure))
	{
		return;
	}

	Adventure->ClosePanel();

	// The last option is Leave; the ones before it are destinations.
	AWaystone* Destination = TravelDestinations.IsValidIndex(OptionIndex) ? TravelDestinations[OptionIndex].Get() : nullptr;

	if (IsValid(Destination))
	{
		Adventure->TeleportPlayer(Destination->GetArrivalTransform());
		Adventure->SetCheckpoint(Destination->GetArrivalTransform());
		Adventure->ShowBanner(Destination->GetDisplayName(), FText::GetEmpty(), 3.0f);
		Adventure->RequestSave();
	}
}

#undef LOCTEXT_NAMESPACE
