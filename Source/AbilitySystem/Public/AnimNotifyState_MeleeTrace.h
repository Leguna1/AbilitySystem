#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_MeleeTrace.generated.h"

class UMeleeTraceComponent;

/** The impact frames of a body attack: the listed parts hit while this is active. */
UCLASS(meta = (DisplayName = "Melee Trace"))
class ABILITYSYSTEM_API UAnimNotifyState_MeleeTrace : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** Shape names from the owner's Melee Trace Component, e.g. RightFist, or LeftFist + RightFist. */
	UPROPERTY(EditAnywhere, Category = "Melee Trace")
	TArray<FName> Shapes;

private:
	static UMeleeTraceComponent* FindTraceComponent(const USkeletalMeshComponent* MeshComp);
};