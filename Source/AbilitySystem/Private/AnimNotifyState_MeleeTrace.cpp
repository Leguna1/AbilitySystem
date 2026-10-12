#include "AnimNotifyState_MeleeTrace.h"

#include "Components/SkeletalMeshComponent.h"
#include "MeleeTraceComponent.h"

void UAnimNotifyState_MeleeTrace::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UMeleeTraceComponent* Trace = FindTraceComponent(MeshComp))
	{
		Trace->BeginTrace(Shapes, Animation);
	}
}

void UAnimNotifyState_MeleeTrace::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (UMeleeTraceComponent* Trace = FindTraceComponent(MeshComp))
	{
		Trace->EndTrace(Shapes);
	}

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UAnimNotifyState_MeleeTrace::GetNotifyName_Implementation() const
{
	return Shapes.IsEmpty()
		? TEXT("Melee Trace")
		: TEXT("Melee Trace: ") + FString::JoinBy(Shapes, TEXT(", "), [](const FName& Name) { return Name.ToString(); });
}

UMeleeTraceComponent* UAnimNotifyState_MeleeTrace::FindTraceComponent(const USkeletalMeshComponent* MeshComp)
{
	// The animation editor's preview actor has none: nothing to do there.
	const AActor* Owner = IsValid(MeshComp) ? MeshComp->GetOwner() : nullptr;
	return IsValid(Owner) ? Owner->FindComponentByClass<UMeleeTraceComponent>() : nullptr;
}