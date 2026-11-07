#include "CharacterPreviewStudio.h"

#include "AbilitySystemLog.h"
#include "Algo/StableSort.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Character.h"
#include "InventoryComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInterface.h"
#include "WeaponManagerComponent.h"

namespace PreviewStudio
{
	/** Studio lights and mirrors live on this channel only; the world uses channel 0. */
	void SetStudioLightingChannel(UPrimitiveComponent* Component)
	{
		Component->SetLightingChannels(false, false, true);
	}

	void SetStudioLightingChannel(ULightComponent* Light)
	{
		Light->SetLightingChannels(false, false, true);
	}

	USpotLightComponent* CreateLight(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const FVector& Location, const float Candelas)
	{
		USpotLightComponent* Light = Owner->CreateDefaultSubobject<USpotLightComponent>(Name);
		Light->SetupAttachment(Parent);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetRelativeLocation(Location);

		// Aim at the middle of a standing character.
		const FVector Target(0.0f, 0.0f, 100.0f);
		Light->SetRelativeRotation((Target - Location).Rotation());

		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(Candelas);
		Light->SetAttenuationRadius(2000.0f);
		Light->SetOuterConeAngle(40.0f);
		Light->SetInnerConeAngle(20.0f);
		SetStudioLightingChannel(Light);
		return Light;
	}

	/** How many components Component hangs under: parents must be mirrored before their children. */
	int32 GetAttachDepth(const USceneComponent* Component)
	{
		int32 Depth = 0;

		for (const USceneComponent* Parent = Component->GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
		{
			++Depth;
		}

		return Depth;
	}

	bool ShouldMirror(const UMeshComponent* Source)
	{
		if (!IsValid(Source) || !Source->IsRegistered() || !Source->ShouldRender() || Source->IsEditorOnly())
		{
			return false;
		}

		// Skeletal parts are copied even while empty (no helmet worn): equipping one later then only sets the
		// mesh on a copy that already follows the body, instead of adding a new copy in the reference pose.
		if (Source->IsA<USkeletalMeshComponent>())
		{
			return true;
		}

		// Instanced meshes (foliage-like props) aren't character parts.
		if (const UStaticMeshComponent* StaticSource = Cast<UStaticMeshComponent>(Source))
		{
			return !Source->IsA<UInstancedStaticMeshComponent>() && IsValid(StaticSource->GetStaticMesh());
		}

		// Niagara, widgets, procedural meshes...: not mirrored.
		return false;
	}

	bool HasMesh(const UMeshComponent* Mirror)
	{
		if (const USkeletalMeshComponent* SkeletalMirror = Cast<USkeletalMeshComponent>(Mirror))
		{
			return IsValid(SkeletalMirror->GetSkeletalMeshAsset());
		}

		if (const UStaticMeshComponent* StaticMirror = Cast<UStaticMeshComponent>(Mirror))
		{
			return IsValid(StaticMirror->GetStaticMesh());
		}

		return false;
	}

	/** The subject's feet and facing: mirrors are placed relative to this, so the subject stands on the pivot facing +X. */
	FTransform GetReferenceTransform(const AActor* SubjectActor)
	{
		FVector Feet = SubjectActor->GetActorLocation();

		if (const ACharacter* Character = Cast<ACharacter>(SubjectActor))
		{
			if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
			{
				Feet.Z -= Capsule->GetScaledCapsuleHalfHeight();
			}
		}

		return FTransform(FRotator(0.0f, SubjectActor->GetActorRotation().Yaw, 0.0f), Feet);
	}

	bool IsAnimationCompatible(const UAnimationAsset* Animation, const USkeletalMesh* MeshAsset)
	{
		if (!IsValid(Animation) || !IsValid(MeshAsset))
		{
			return false;
		}

		const USkeleton* AnimSkeleton = Animation->GetSkeleton();
		return IsValid(AnimSkeleton) && (AnimSkeleton == MeshAsset->GetSkeleton() || AnimSkeleton->IsCompatibleMesh(MeshAsset));
	}
}

/* -------------------- Lifetime -------------------- */

ACharacterPreviewStudio::ACharacterPreviewStudio()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// The menu pauses the game; the preview still has to animate, refresh and capture.
	PrimaryActorTick.bTickEvenWhenPaused = true;

	SetCanBeDamaged(false);

	StudioRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StudioRoot"));
	StudioRoot->SetMobility(EComponentMobility::Movable);
	SetRootComponent(StudioRoot);

	SubjectPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SubjectPivot"));
	SubjectPivot->SetupAttachment(StudioRoot);
	SubjectPivot->SetMobility(EComponentMobility::Movable);

	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureComponent"));
	CaptureComponent->SetupAttachment(StudioRoot);
	CaptureComponent->SetRelativeLocation(FVector(400.0f, 0.0f, 100.0f));
	CaptureComponent->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	CaptureComponent->FOVAngle = 30.0f;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	CaptureComponent->PrimaryComponentTick.bTickEvenWhenPaused = true;

	// Only the studio's own lights: no world fog, sky, atmosphere, clouds or world GI leaking in.
	FEngineShowFlags& Flags = CaptureComponent->ShowFlags;
	Flags.SetFog(false);
	Flags.SetVolumetricFog(false);
	Flags.SetAtmosphere(false);
	Flags.SetCloud(false);
	Flags.SetSkyLighting(false);
	Flags.SetGlobalIllumination(false);
	Flags.SetLumenGlobalIllumination(false);
	Flags.SetLumenReflections(false);
	Flags.SetDistanceFieldAO(false);
	Flags.SetScreenSpaceReflections(false);
	Flags.SetReflectionEnvironment(false);
	Flags.SetDecals(false);
	Flags.SetParticles(false);
	Flags.SetMotionBlur(false);
	Flags.SetBloom(false);
	Flags.SetEyeAdaptation(false);

	// Fixed exposure, so the preview doesn't brighten and darken as the camera moves.
	FPostProcessSettings& PostProcess = CaptureComponent->PostProcessSettings;
	PostProcess.bOverride_AutoExposureMethod = true;
	PostProcess.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	PostProcess.bOverride_AutoExposureBias = true;
	PostProcess.AutoExposureBias = 0.0f;
	PostProcess.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	PostProcess.AutoExposureApplyPhysicalCameraExposure = false;
	PostProcess.bOverride_DynamicGlobalIlluminationMethod = true;
	PostProcess.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	PostProcess.bOverride_ReflectionMethod = true;
	PostProcess.ReflectionMethod = EReflectionMethod::None;
	CaptureComponent->PostProcessBlendWeight = 1.0f;

	// Three-point rig around a subject facing +X, where the camera is.
	KeyLight = PreviewStudio::CreateLight(this, StudioRoot, TEXT("KeyLight"), FVector(300.0f, -250.0f, 300.0f), 60.0f);
	FillLight = PreviewStudio::CreateLight(this, StudioRoot, TEXT("FillLight"), FVector(300.0f, 300.0f, 120.0f), 20.0f);
	RimLight = PreviewStudio::CreateLight(this, StudioRoot, TEXT("RimLight"), FVector(-300.0f, 100.0f, 280.0f), 50.0f);
	FillLight->SetCastShadows(false);
}

void ACharacterPreviewStudio::BeginPlay()
{
	Super::BeginPlay();

	RenderTarget = NewObject<UTextureRenderTarget2D>(this, TEXT("PreviewRenderTarget"));
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;

	// Alpha 1 = nothing there, in Scene Color HDR's inverted alpha: starts out transparent in the UI.
	RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);
	RenderTarget->InitAutoFormat(FMath::Max(RenderTargetSize.X, 64), FMath::Max(RenderTargetSize.Y, 64));
	RenderTarget->UpdateResourceImmediate(true);

	CaptureComponent->TextureTarget = RenderTarget;

	ApplyLightsVisible(false);
}

void ACharacterPreviewStudio::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindSubject();
	ClearMirror();
	Super::EndPlay(EndPlayReason);
}

void ACharacterPreviewStudio::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The subject went away (companion died, player respawned) while shown.
	if (!Subject.IsValid() && !Mirrors.IsEmpty())
	{
		UnbindSubject();
		RebuildMirror();
		return;
	}

	// The mirrors made last frame have ticked their animation by the end of this one: capture again.
	if (CaptureHoldFrames > 0 && --CaptureHoldFrames == 0)
	{
		UpdateCaptureEveryFrame();
	}

	// Deferred by a tick so Blueprint handlers of the same event (material changes) have run too.
	if (bMirrorDirty)
	{
		RebuildMirror();
	}
}

/* -------------------- Subject -------------------- */

void ACharacterPreviewStudio::SetSubject(AActor* NewSubject)
{
	if (NewSubject == Subject.Get() && !bMirrorDirty)
	{
		return;
	}

	const bool bNewSubject = NewSubject != Subject.Get();

	UnbindSubject();
	Subject = NewSubject;
	BindSubject();

	if (bNewSubject)
	{
		ResetSubjectRotation();
	}

	RebuildMirror();
}

void ACharacterPreviewStudio::BindSubject()
{
	AActor* SubjectActor = Subject.Get();

	if (!IsValid(SubjectActor))
	{
		return;
	}

	if (UInventoryComponent* Inventory = SubjectActor->FindComponentByClass<UInventoryComponent>())
	{
		Inventory->OnEquipmentChanged.AddUniqueDynamic(this, &ACharacterPreviewStudio::HandleEquipmentChanged);
		BoundInventory = Inventory;
	}

	if (UWeaponManagerComponent* Weapons = SubjectActor->FindComponentByClass<UWeaponManagerComponent>())
	{
		Weapons->OnWeaponChanged.AddUniqueDynamic(this, &ACharacterPreviewStudio::HandleWeaponChanged);
		BoundWeapons = Weapons;
	}
}

void ACharacterPreviewStudio::UnbindSubject()
{
	if (UInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnEquipmentChanged.RemoveDynamic(this, &ACharacterPreviewStudio::HandleEquipmentChanged);
	}

	if (UWeaponManagerComponent* Weapons = BoundWeapons.Get())
	{
		Weapons->OnWeaponChanged.RemoveDynamic(this, &ACharacterPreviewStudio::HandleWeaponChanged);
	}

	BoundInventory.Reset();
	BoundWeapons.Reset();
}

void ACharacterPreviewStudio::HandleEquipmentChanged(int32 EquipmentSlotIndex, UEquipmentDataAsset* PreviousItem, UEquipmentDataAsset* NewItem)
{
	MarkMirrorDirty();
}

void ACharacterPreviewStudio::HandleWeaponChanged(UWeaponDataAsset* PreviousWeaponData, AWeaponBase* NewWeapon)
{
	MarkMirrorDirty();
}

/* -------------------- Capture -------------------- */

void ACharacterPreviewStudio::SetCaptureActive(const bool bActive)
{
	bCaptureActive = bActive;

	UpdateCaptureEveryFrame();
	SetActorTickEnabled(bActive);
	ApplyLightsVisible(bActive);

	if (bActive && bMirrorDirty)
	{
		RebuildMirror();
	}
}

void ACharacterPreviewStudio::UpdateCaptureEveryFrame() const
{
	CaptureComponent->bCaptureEveryFrame = bCaptureActive && CaptureHoldFrames == 0;
}

void ACharacterPreviewStudio::ApplyLightsVisible(const bool bVisible) const
{
	for (USpotLightComponent* Light : { KeyLight.Get(), FillLight.Get(), RimLight.Get() })
	{
		if (IsValid(Light))
		{
			Light->SetVisibility(bVisible);
		}
	}
}

void ACharacterPreviewStudio::AddSubjectYaw(const float DeltaYaw)
{
	SubjectPivot->AddRelativeRotation(FRotator(0.0f, DeltaYaw, 0.0f));
}

void ACharacterPreviewStudio::ResetSubjectRotation()
{
	SubjectPivot->SetRelativeRotation(FRotator::ZeroRotator);
}

/* -------------------- Mirroring -------------------- */

void ACharacterPreviewStudio::DestroyMirror(UMeshComponent* Mirror)
{
	if (!IsValid(Mirror))
	{
		return;
	}

	if (IsValid(CaptureComponent))
	{
		CaptureComponent->RemoveShowOnlyComponent(Mirror);
	}

	Mirror->DestroyComponent();
}

void ACharacterPreviewStudio::ClearMirror()
{
	for (const FPreviewMirror& Entry : Mirrors)
	{
		DestroyMirror(Entry.Mirror);
	}

	if (IsValid(CaptureComponent))
	{
		CaptureComponent->ClearShowOnlyComponents();
	}

	Mirrors.Reset();
	MirrorBody = nullptr;
}

void ACharacterPreviewStudio::RebuildMirror()
{
	bMirrorDirty = false;

	AActor* SubjectActor = Subject.Get();

	if (!IsValid(SubjectActor))
	{
		ClearMirror();

		// Nothing to show: make the image transparent instead of keeping the last subject.
		if (IsValid(RenderTarget))
		{
			UKismetRenderingLibrary::ClearRenderTarget2D(this, RenderTarget, RenderTarget->ClearColor);
		}
		return;
	}

	// The subject's own meshes, then those of every actor attached to it (weapon, quiver, an arrow on the bow...).
	TArray<UMeshComponent*> Sources;

	const auto GatherMeshes = [&Sources](const AActor* Actor)
	{
		if (!IsValid(Actor) || Actor->IsHidden())
		{
			return;
		}

		TInlineComponentArray<UMeshComponent*> Meshes(Actor);

		for (UMeshComponent* Mesh : Meshes)
		{
			if (PreviewStudio::ShouldMirror(Mesh))
			{
				Sources.Add(Mesh);
			}
		}
	};

	GatherMeshes(SubjectActor);

	TArray<AActor*> AttachedActors;
	SubjectActor->GetAttachedActors(AttachedActors, true, true);

	for (const AActor* AttachedActor : AttachedActors)
	{
		GatherMeshes(AttachedActor);
	}

	// Parents first, so every child finds its parent's mirror to attach to.
	Algo::StableSortBy(Sources, [](const UMeshComponent* Source) { return PreviewStudio::GetAttachDepth(Source); });

	const FTransform ReferenceTransform = PreviewStudio::GetReferenceTransform(SubjectActor);
	TMap<USceneComponent*, UMeshComponent*> MirrorsBySource;

	// Each source keeps the copy it had, so an equipment swap only changes the mesh on an already
	// animated copy. Copies made from scratch would stand in the reference pose for a frame.
	TArray<FPreviewMirror> PreviousMirrors = MoveTemp(Mirrors);
	Mirrors.Reset();
	bool bCreatedSkeletalMirror = false;

	for (UMeshComponent* Source : Sources)
	{
		const bool bSkeletalSource = Source->IsA<USkeletalMeshComponent>();
		UMeshComponent* Existing = nullptr;

		const int32 PreviousIndex = PreviousMirrors.IndexOfByPredicate([Source, bSkeletalSource](const FPreviewMirror& Entry)
		{
			return Entry.Source.Get() == Source && IsValid(Entry.Mirror) && Entry.Mirror->IsA<USkeletalMeshComponent>() == bSkeletalSource;
		});

		if (PreviousIndex != INDEX_NONE)
		{
			Existing = PreviousMirrors[PreviousIndex].Mirror;
			PreviousMirrors.RemoveAtSwap(PreviousIndex);
		}

		if (UMeshComponent* Mirror = UpdateMirror(Existing, Source, MirrorsBySource, ReferenceTransform))
		{
			if (!Existing)
			{
				CaptureComponent->ShowOnlyComponent(Mirror);
				bCreatedSkeletalMirror |= bSkeletalSource;
			}

			MirrorsBySource.Add(Source, Mirror);
			Mirrors.Add({ Source, Mirror });
		}
	}

	// Sources that went away (a weapon put away, another subject): their copies go too.
	for (const FPreviewMirror& Entry : PreviousMirrors)
	{
		DestroyMirror(Entry.Mirror);
	}

	// The body: the character's main mesh, else the first skeletal copy that has a mesh.
	MirrorBody = nullptr;

	if (const ACharacter* SubjectCharacter = Cast<ACharacter>(SubjectActor))
	{
		if (UMeshComponent* const* BodyMirror = MirrorsBySource.Find(SubjectCharacter->GetMesh()))
		{
			MirrorBody = Cast<USkeletalMeshComponent>(*BodyMirror);
		}
	}

	if (!IsValid(MirrorBody))
	{
		for (const FPreviewMirror& Entry : Mirrors)
		{
			USkeletalMeshComponent* SkeletalMirror = Cast<USkeletalMeshComponent>(Entry.Mirror);

			if (IsValid(SkeletalMirror) && IsValid(SkeletalMirror->GetSkeletalMeshAsset()))
			{
				MirrorBody = SkeletalMirror;
				break;
			}
		}
	}

	ApplyLeaderPoses(MirrorsBySource);
	ApplyAnimation();
	FrameSubject();

	// New skeletal copies (first show, a new subject, a skeletal weapon) stand in the reference pose until
	// their first tick, next frame: keep the previous image until then. In-place swaps don't need this.
	if (bCreatedSkeletalMirror)
	{
		CaptureHoldFrames = 1;
		UpdateCaptureEveryFrame();
	}

	OnMirrorRebuilt();
}

UMeshComponent* ACharacterPreviewStudio::UpdateMirror(UMeshComponent* Mirror, UMeshComponent* Source, const TMap<USceneComponent*, UMeshComponent*>& MirrorsBySource, const FTransform& ReferenceTransform)
{
	const bool bNew = !IsValid(Mirror);

	if (bNew)
	{
		if (Source->IsA<USkeletalMeshComponent>())
		{
			USkeletalMeshComponent* SkeletalMirror = NewObject<USkeletalMeshComponent>(this);

			// It's never seen by a player camera, so "only animate when rendered" would freeze it.
			SkeletalMirror->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			SkeletalMirror->bEnableUpdateRateOptimizations = false;
			Mirror = SkeletalMirror;
		}
		else if (Source->IsA<UStaticMeshComponent>())
		{
			UStaticMeshComponent* StaticMirror = NewObject<UStaticMeshComponent>(this);
			StaticMirror->SetMobility(EComponentMobility::Movable);
			Mirror = StaticMirror;
		}

		if (!IsValid(Mirror))
		{
			return nullptr;
		}

		Mirror->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mirror->SetGenerateOverlapEvents(false);
		Mirror->SetCanEverAffectNavigation(false);
		Mirror->SetReceivesDecals(false);
		Mirror->SetCastShadow(true);
		Mirror->SetVisibleInSceneCaptureOnly(true);
		Mirror->PrimaryComponentTick.bTickEvenWhenPaused = true;
		PreviewStudio::SetStudioLightingChannel(Mirror);
	}

	// On a reused copy this is the in-place swap: the animated body keeps its pose, and a follower
	// (a gear part) takes its leader's pose straight away.
	if (const USkeletalMeshComponent* SkeletalSource = Cast<USkeletalMeshComponent>(Source))
	{
		USkeletalMeshComponent* SkeletalMirror = CastChecked<USkeletalMeshComponent>(Mirror);

		if (SkeletalMirror->GetSkeletalMeshAsset() != SkeletalSource->GetSkeletalMeshAsset())
		{
			SkeletalMirror->SetSkeletalMeshAsset(SkeletalSource->GetSkeletalMeshAsset());
		}
	}
	else if (const UStaticMeshComponent* StaticSource = Cast<UStaticMeshComponent>(Source))
	{
		CastChecked<UStaticMeshComponent>(Mirror)->SetStaticMesh(StaticSource->GetStaticMesh());
	}

	// Attach to the nearest mirrored ancestor, at the socket the chain hangs from (a weapon actor's root
	// in hand_r: its meshes go on the mirrored body's hand_r). Offsets are kept relative to that socket,
	// so the weapon sits in the idle pose's hand, not where the gameplay pose had it.
	const USceneComponent* Child = Source;
	USceneComponent* MirroredAncestor = nullptr;
	UMeshComponent* AncestorMirror = nullptr;

	for (USceneComponent* Ancestor = Source->GetAttachParent(); Ancestor; Ancestor = Ancestor->GetAttachParent())
	{
		if (UMeshComponent* const* Found = MirrorsBySource.Find(Ancestor))
		{
			MirroredAncestor = Ancestor;
			AncestorMirror = *Found;
			break;
		}

		Child = Ancestor;
	}

	USceneComponent* MirrorParent = SubjectPivot;
	FName SocketName = NAME_None;
	FTransform RelativeTransform = Source->GetComponentTransform().GetRelativeTransform(ReferenceTransform);

	if (IsValid(AncestorMirror))
	{
		MirrorParent = AncestorMirror;
		SocketName = Child->GetAttachSocketName();
		RelativeTransform = Source->GetComponentTransform().GetRelativeTransform(MirroredAncestor->GetSocketTransform(SocketName));
	}

	if (bNew)
	{
		Mirror->SetupAttachment(MirrorParent, SocketName);
		Mirror->SetRelativeTransform(RelativeTransform);
		Mirror->RegisterComponent();
	}
	else
	{
		if (Mirror->GetAttachParent() != MirrorParent || Mirror->GetAttachSocketName() != SocketName)
		{
			Mirror->AttachToComponent(MirrorParent, FAttachmentTransformRules::KeepRelativeTransform, SocketName);
		}

		Mirror->SetRelativeTransform(RelativeTransform);
	}

	// Materials as the source shows them right now: overrides, and the very same dynamic material
	// instances, so tier recolours and glows made through parameters update in the preview by themselves.
	for (int32 MaterialIndex = 0; MaterialIndex < Source->GetNumMaterials(); ++MaterialIndex)
	{
		Mirror->SetMaterial(MaterialIndex, Source->GetMaterial(MaterialIndex));
	}

	Mirror->SetOverlayMaterial(Source->GetOverlayMaterial());
	Mirror->SetOverlayMaterialMaxDrawDistance(Source->GetOverlayMaterialMaxDrawDistance());

	const TArray<float>& CustomData = Source->GetCustomPrimitiveData().Data;

	for (int32 DataIndex = 0; DataIndex < CustomData.Num(); ++DataIndex)
	{
		Mirror->SetCustomPrimitiveDataFloat(DataIndex, CustomData[DataIndex]);
	}

	return Mirror;
}

void ACharacterPreviewStudio::ApplyLeaderPoses(const TMap<USceneComponent*, UMeshComponent*>& MirrorsBySource)
{
	const USkeleton* BodySkeleton = IsValid(MirrorBody) && IsValid(MirrorBody->GetSkeletalMeshAsset())
		? MirrorBody->GetSkeletalMeshAsset()->GetSkeleton()
		: nullptr;

	for (const TPair<USceneComponent*, UMeshComponent*>& Pair : MirrorsBySource)
	{
		const USkeletalMeshComponent* SkeletalSource = Cast<USkeletalMeshComponent>(Pair.Key);
		USkeletalMeshComponent* SkeletalMirror = Cast<USkeletalMeshComponent>(Pair.Value);

		if (!IsValid(SkeletalSource) || !IsValid(SkeletalMirror))
		{
			continue;
		}

		USkinnedMeshComponent* WantedLeader = nullptr;

		if (SkeletalMirror != MirrorBody)
		{
			// Same leader as in the game.
			if (USkinnedMeshComponent* SourceLeader = SkeletalSource->LeaderPoseComponent.Get())
			{
				if (UMeshComponent* const* LeaderMirror = MirrorsBySource.Find(SourceLeader))
				{
					WantedLeader = Cast<USkinnedMeshComponent>(*LeaderMirror);
				}
			}

			// Parts that copied the body's pose some other way (a Copy Pose anim graph) aren't animated here:
			// if they share the body's skeleton, have them follow the body instead of standing in the reference pose.
			const USkeletalMesh* PartMesh = SkeletalMirror->GetSkeletalMeshAsset();

			if (!WantedLeader && BodySkeleton && IsValid(PartMesh) && PartMesh->GetSkeleton() == BodySkeleton)
			{
				WantedLeader = MirrorBody;
			}
		}

		// Reused copies keep a leader that's still right; changing it every rebuild would re-map bones for nothing.
		if (SkeletalMirror->LeaderPoseComponent.Get() != WantedLeader)
		{
			SkeletalMirror->SetLeaderPoseComponent(WantedLeader);
		}
	}
}

UAnimationAsset* ACharacterPreviewStudio::FindAnimationFor(const USkeletalMesh* MeshAsset) const
{
	if (!IsValid(MeshAsset))
	{
		return nullptr;
	}

	// The subject's own choice first.
	if (const ABaseCharacter* SubjectCharacter = Cast<ABaseCharacter>(Subject.Get()))
	{
		UAnimationAsset* OwnAnimation = SubjectCharacter->GetPreviewAnimation();

		if (PreviewStudio::IsAnimationCompatible(OwnAnimation, MeshAsset))
		{
			return OwnAnimation;
		}

		if (IsValid(OwnAnimation))
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("%s: Preview Animation %s doesn't fit mesh %s; using the studio's fallback."),
				*GetNameSafe(SubjectCharacter), *GetNameSafe(OwnAnimation), *GetNameSafe(MeshAsset));
		}
	}

	if (const TObjectPtr<UAnimationAsset>* Fallback = FallbackAnimations.Find(MeshAsset->GetSkeleton()))
	{
		if (PreviewStudio::IsAnimationCompatible(*Fallback, MeshAsset))
		{
			return *Fallback;
		}
	}

	return nullptr;
}

void ACharacterPreviewStudio::ApplyAnimation()
{
	if (!IsValid(MirrorBody))
	{
		return;
	}

	// No animation: the mirror has no Anim Blueprint either, so it stands in the reference pose.
	UAnimationAsset* Animation = FindAnimationFor(MirrorBody->GetSkeletalMeshAsset());

	if (!IsValid(Animation))
	{
		return;
	}

	// Already playing it (a reused body after a gear swap): carry on, so the idle doesn't restart.
	const UAnimSingleNodeInstance* SingleNode = MirrorBody->GetSingleNodeInstance();

	if (MirrorBody->GetAnimationMode() == EAnimationMode::AnimationSingleNode && SingleNode && SingleNode->GetAnimationAsset() == Animation)
	{
		return;
	}

	MirrorBody->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	MirrorBody->PlayAnimation(Animation, true);
}

void ACharacterPreviewStudio::FrameSubject()
{
	FBox Box(ForceInit);

	for (const FPreviewMirror& Entry : Mirrors)
	{
		// Empty gear parts have no mesh: their zero-size bounds would only pull the frame off.
		if (PreviewStudio::HasMesh(Entry.Mirror))
		{
			Box += Entry.Mirror->Bounds.GetBox();
		}
	}

	if (!Box.IsValid)
	{
		return;
	}

	// Bounds are world-space; turning the subject doesn't move the camera, so frame for its widest side.
	const FVector Size = Box.GetSize();
	const float Height = Size.Z;
	const float Width = FMath::Max(Size.X, Size.Y);

	const float Aspect = RenderTargetSize.Y > 0 ? static_cast<float>(RenderTargetSize.X) / RenderTargetSize.Y : 1.0f;
	const float TanHalfHorizontal = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(CaptureComponent->FOVAngle, 5.0f, 170.0f) * 0.5f));
	const float TanHalfVertical = TanHalfHorizontal / FMath::Max(Aspect, KINDA_SMALL_NUMBER);

	const float FitHeight = (Height * 0.5f) / TanHalfVertical;
	const float FitWidth = (Width * 0.5f) / TanHalfHorizontal;

	// Measured from the front of the subject, not its middle.
	const float Distance = FMath::Max(FMath::Max(FitHeight, FitWidth) * FramingPadding + Width * 0.5f, MinCameraDistance);

	const FVector Focus = FVector(SubjectPivot->GetComponentLocation().X, SubjectPivot->GetComponentLocation().Y, Box.GetCenter().Z + FocusHeightOffset);
	const FVector Forward = GetActorForwardVector();

	CaptureComponent->SetWorldLocationAndRotation(Focus + Forward * Distance, (-Forward).Rotation());
}
