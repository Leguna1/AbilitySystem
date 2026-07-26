#include "AnimNotify_AbilityEvent.h"

#include "AbilityComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UAnimNotify_AbilityEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp) || !EventTag.IsValid())
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();

	if (!IsValid(Owner))
	{
		return;
	}

	if (UAbilityComponent* AbilityComponent = Owner->FindComponentByClass<UAbilityComponent>())
	{
		AbilityComponent->HandleAbilityEvent(EventTag);
	}
}

FString UAnimNotify_AbilityEvent::GetNotifyName_Implementation() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}

	if (EventTag.IsValid())
	{
		return EventTag.GetTagName().ToString();
	}
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("Ability Event");
}