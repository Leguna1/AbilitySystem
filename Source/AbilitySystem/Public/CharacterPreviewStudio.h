#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CharacterPreviewStudio.generated.h"

class AWeaponBase;
class UAnimationAsset;
class UEquipmentDataAsset;
class UInventoryComponent;
class UMeshComponent;
class USceneCaptureComponent2D;
class USkeletalMesh;
class USkeletalMeshComponent;
class USkeleton;
class USpotLightComponent;
class UTextureRenderTarget2D;
class UWeaponDataAsset;
class UWeaponManagerComponent;

/** One of the studio's copies and the subject's component it copies. */
USTRUCT()
struct FPreviewMirror
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> Source;

	UPROPERTY()
	TObjectPtr<UMeshComponent> Mirror;
};

/**
 * A small photo studio for the character menu's 3D preview. It is spawned far from the playable world and
 * holds copies ("mirrors") of a subject's meshes (the player or the companion), its own lights,
 * and a scene capture writing to a render target the menu shows in an Image.
 *
 *   - Mirrors: every visible skeletal and static mesh component of the subject and of every actor attached
 *     to it (weapon, quiver, anything), with their materials (dynamic material instances are shared, so
 *     parameter changes show live), overlay material, custom primitive data, attachment and leader pose.
 *   - Animation: the subject's Preview Animation (Base Character), else Fallback Animations for its skeleton,
 *     else the reference pose. Never the gameplay Anim Blueprint.
 *   - Lighting: mirrors and studio lights are on lighting channel 2 only, so the world's lights don't
 *     touch the preview and the studio's lights don't touch the world. Mirrors are visible to scene
 *     captures only, never to the game camera.
 *   - Capture: show-only list, Scene Color HDR (alpha = 1 - coverage: invert it in the UI material for a
 *     transparent background). Every frame only while active; also while the game is paused.
 *   - Refresh: on the next tick after the subject's equipment or weapon changes. Copies are updated in place
 *     (a gear swap only changes the mesh on an already-animated copy); only new or removed parts add or drop copies.
 *
 * Make a Blueprint of it to tune the lights, the capture's post process and the framing.
 */
UCLASS(Blueprintable)
class ABILITYSYSTEM_API ACharacterPreviewStudio : public AActor
{
	GENERATED_BODY()

public:
	ACharacterPreviewStudio();

	virtual void Tick(float DeltaSeconds) override;

	/** Mirrors NewSubject (null clears the studio). Rotation is reset for a new subject. */
	UFUNCTION(BlueprintCallable, Category = "Preview Studio")
	void SetSubject(AActor* NewSubject);

	UFUNCTION(BlueprintPure, Category = "Preview Studio")
	AActor* GetSubject() const { return Subject.Get(); }

	/** Capture every frame (true) or not at all (false). On while the preview is on screen. */
	UFUNCTION(BlueprintCallable, Category = "Preview Studio")
	void SetCaptureActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Preview Studio")
	bool IsCaptureActive() const { return bCaptureActive; }

	/**
	 * Re-mirrors on the next tick. Equipment and weapon changes call this already; call it yourself after
	 * changing the subject's look some other way (a new material, a mesh swapped in Blueprint).
	 */
	UFUNCTION(BlueprintCallable, Category = "Preview Studio")
	void MarkMirrorDirty() { bMirrorDirty = true; }

	/** Turns the subject around its feet (drag in the preview). */
	UFUNCTION(BlueprintCallable, Category = "Preview Studio")
	void AddSubjectYaw(float DeltaYaw);

	UFUNCTION(BlueprintCallable, Category = "Preview Studio")
	void ResetSubjectRotation();

	/** What the capture writes to. Shown by Character Preview Widgets. Created on Begin Play. */
	UFUNCTION(BlueprintPure, Category = "Preview Studio")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/* -------------------- Components -------------------- */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> StudioRoot;

	/** The subject's feet. Mirrors hang under it; dragging turns it. The subject faces +X (towards the camera). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SubjectPivot;

	/** Placed automatically to frame the subject; tune its post process and show flags here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpotLightComponent> RimLight;

	/* -------------------- Settings -------------------- */

	/** Render target size in pixels. Match the aspect of the Image showing it (tall for a standing character). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview Studio", meta = (ClampMin = "64"))
	FIntPoint RenderTargetSize = FIntPoint(1024, 1536);

	/** Empty space around the subject: 1 = touching the edges, 1.15 = 15% margin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview Studio|Framing", meta = (ClampMin = "1.0"))
	float FramingPadding = 1.15f;

	/** Never closer than this, so tiny subjects don't fill the frame with a blurry close-up. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview Studio|Framing", meta = (ClampMin = "10.0", Units = "cm"))
	float MinCameraDistance = 150.0f;

	/** Looks this much above (+) or below (-) the middle of the subject. Positive frames the head higher. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview Studio|Framing", meta = (Units = "cm"))
	float FocusHeightOffset = 0.0f;

	/**
	 * Played when the subject has no Preview Animation of its own (or it doesn't fit its skeleton).
	 * Key: the skeleton of the subject's main mesh. Value: a looping idle for it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Preview Studio|Animation")
	TMap<TObjectPtr<USkeleton>, TObjectPtr<UAnimationAsset>> FallbackAnimations;

	/** The mirror was rebuilt (new subject, equipment change). Add extras (a pedestal, particles) here. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Preview Studio")
	void OnMirrorRebuilt();

private:
	UFUNCTION()
	void HandleEquipmentChanged(int32 EquipmentSlotIndex, UEquipmentDataAsset* PreviousItem, UEquipmentDataAsset* NewItem);

	UFUNCTION()
	void HandleWeaponChanged(UWeaponDataAsset* PreviousWeaponData, AWeaponBase* NewWeapon);

	void BindSubject();
	void UnbindSubject();

	void RebuildMirror();
	void ClearMirror();

	/**
	 * Makes Mirror (or a new copy, when null) look like Source: mesh, attachment, materials. Null if Source
	 * isn't a mesh the studio copies.
	 */
	UMeshComponent* UpdateMirror(UMeshComponent* Mirror, UMeshComponent* Source, const TMap<USceneComponent*, UMeshComponent*>& MirrorsBySource, const FTransform& ReferenceTransform);

	void DestroyMirror(UMeshComponent* Mirror);

	/** Leader pose: followers follow the mirrored leader; same-skeleton parts without one follow the body. */
	void ApplyLeaderPoses(const TMap<USceneComponent*, UMeshComponent*>& MirrorsBySource);

	void ApplyAnimation();
	UAnimationAsset* FindAnimationFor(const USkeletalMesh* MeshAsset) const;

	/** Moves the capture so the mirrored subject fills the frame. */
	void FrameSubject();

	void ApplyLightsVisible(bool bVisible) const;

	/** Captures every frame only while active and not holding the last image for freshly rebuilt mirrors. */
	void UpdateCaptureEveryFrame() const;

	TWeakObjectPtr<AActor> Subject;
	TWeakObjectPtr<UInventoryComponent> BoundInventory;
	TWeakObjectPtr<UWeaponManagerComponent> BoundWeapons;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	/** Every copy with the component it copies, so a rebuild can update copies in place. Parents first. */
	UPROPERTY(Transient)
	TArray<FPreviewMirror> Mirrors;

	/** The mirror of the subject's main mesh: plays the preview animation, others follow it. */
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> MirrorBody;

	bool bCaptureActive = false;
	bool bMirrorDirty = false;

	/**
	 * Frames left to keep showing the previous image after a rebuild. New mirrors are created in the
	 * reference pose and only get animated by their first tick, the frame after; capturing before that
	 * flashes a T-posed body whose leader-posed gear isn't drawn yet.
	 */
	int32 CaptureHoldFrames = 0;
};
