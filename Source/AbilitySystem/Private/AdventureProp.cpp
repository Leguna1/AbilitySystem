#include "AdventureProp.h"

#include "AdventureComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "WorldProgressSubsystem.h"

AAdventureProp::AAdventureProp()
{
	// Only to turn the label toward the camera; twenty times a second is plenty.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(SceneRoot);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(SceneRoot);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetWorldSize(28.0f);
	Label->SetTextRenderColor(FColor::White);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(SceneRoot);
	Glow->SetIntensity(3000.0f);
	Glow->SetAttenuationRadius(500.0f);
	Glow->SetCastShadows(false);

	SetPlaceholder(EAdventurePropShape::Cube, FVector(1.0f), 150.0f);
}

void AAdventureProp::SetPlaceholder(const EAdventurePropShape Shape, const FVector& Scale, const float LabelHeight)
{
	const TCHAR* Path = TEXT("/Engine/BasicShapes/Cube.Cube");

	switch (Shape)
	{
	case EAdventurePropShape::Sphere:
		Path = TEXT("/Engine/BasicShapes/Sphere.Sphere");
		break;

	case EAdventurePropShape::Cylinder:
		Path = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
		break;

	case EAdventurePropShape::Cone:
		Path = TEXT("/Engine/BasicShapes/Cone.Cone");
		break;

	default:
		break;
	}

	// Constructor-only: this is called from the constructors of this class and its children.
	ConstructorHelpers::FObjectFinder<UStaticMesh> Finder(Path);

	if (Finder.Succeeded())
	{
		Mesh->SetStaticMesh(Finder.Object);
	}

	// The basic shapes are 100 units across with the origin in the middle: lifted so they stand on the ground.
	Mesh->SetRelativeScale3D(Scale);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f * Scale.Z));

	Label->SetRelativeLocation(FVector(0.0f, 0.0f, LabelHeight));
	Glow->SetRelativeLocation(FVector(0.0f, 0.0f, LabelHeight * 0.6f));
}

void AAdventureProp::BeginPlay()
{
	Super::BeginPlay();

	if (UWorldProgressSubsystem* Progress = GetProgress())
	{
		Progress->RegisterInteractable(this);
	}

	RefreshLabel();
}

void AAdventureProp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorldProgressSubsystem* Progress = GetProgress())
	{
		Progress->UnregisterInteractable(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AAdventureProp::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bShowLabel || !IsValid(Label))
	{
		return;
	}

	// Text Render doesn't face the camera by itself.
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const FVector ToCamera = Camera->GetCameraLocation() - Label->GetComponentLocation();
		Label->SetWorldRotation(FRotator(0.0f, ToCamera.Rotation().Yaw, 0.0f));
	}
}

UAdventureComponent* AAdventureProp::GetAdventure() const
{
	return UAdventureComponent::Get(this);
}

UWorldProgressSubsystem* AAdventureProp::GetProgress() const
{
	return UWorldProgressSubsystem::Get(this);
}

void AAdventureProp::SetStateColor(const FLinearColor Color)
{
	if (IsValid(Glow))
	{
		Glow->SetLightColor(Color);
	}

	if (IsValid(Label))
	{
		Label->SetTextRenderColor(Color.ToFColor(true));
	}

	// The engine's basic shape material has a Color parameter; on your own materials this does nothing.
	if (IsValid(Mesh) && Mesh->GetNumMaterials() > 0)
	{
		if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateDynamicMaterialInstance(0))
		{
			Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

void AAdventureProp::SetGlowEnabled(const bool bEnabled)
{
	if (IsValid(Glow))
	{
		Glow->SetVisibility(bEnabled);
	}
}

void AAdventureProp::SetLabelStatus(const FText& Status)
{
	LabelStatus = Status;
	RefreshLabel();
}

void AAdventureProp::RefreshLabel()
{
	if (!IsValid(Label))
	{
		return;
	}

	Label->SetVisibility(bShowLabel);

	const FString Text = LabelStatus.IsEmpty()
		? DisplayName.ToString()
		: FString::Printf(TEXT("%s<br>%s"), *DisplayName.ToString(), *LabelStatus.ToString());

	Label->SetText(FText::FromString(Text));
}
