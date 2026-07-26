#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_AbilityEvent.generated.h"

UCLASS(meta = (DisplayName = "Ability Event"))
class ABILITYSYSTEM_API UAnimNotify_AbilityEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
	FGameplayTag EventTag;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Event")
	FString DisplayName;
};