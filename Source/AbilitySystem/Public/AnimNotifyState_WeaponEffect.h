#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_WeaponEffect.generated.h"

class UNiagaraSystem;

/**
 * Plays a Niagara effect attached to the wielder's equipped weapon for the
 * notify window. Ends with the window or when the montage stops.
 *
 * In editor previews (no weapon manager) the effect attaches to the previewed
 * mesh instead, so timing can still be authored.
 */
UCLASS(meta = (DisplayName = "Weapon Effect"))
class ABILITYSYSTEM_API UAnimNotifyState_WeaponEffect : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, Category = "Weapon Effect")
	TObjectPtr<UNiagaraSystem> Effect;

	/** Socket on any of the weapon's meshes. None = the weapon's root. */
	UPROPERTY(EditAnywhere, Category = "Weapon Effect")
	FName SocketName = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Weapon Effect")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Weapon Effect")
	FRotator RotationOffset = FRotator::ZeroRotator;

	/** When the window ends, let particles finish instead of cutting them off. */
	UPROPERTY(EditAnywhere, Category = "Weapon Effect")
	bool bLetParticlesFinish = true;

private:
	USceneComponent* FindAttachComponent(USkeletalMeshComponent* MeshComp) const;

	/** Tags spawned components so NotifyEnd finds them without per-instance state. */
	FName GetEffectTag() const;
};