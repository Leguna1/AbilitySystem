#pragma once

#include "CoreMinimal.h"
#include "MontageAbility.h"
#include "HitReactionTypes.h"
#include "HitReactionAbility.generated.h"

class UAnimMontage;

/** One montage per side a hit can come from. Missing sides fall back to Front. */
USTRUCT(BlueprintType)
struct FHitReactionMontages
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> Front;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> Back;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> Left;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Reaction")
	TObjectPtr<UAnimMontage> Right;

	UAnimMontage* Get(const EHitDirection Direction) const
	{
		UAnimMontage* Montage = nullptr;

		switch (Direction)
		{
		case EHitDirection::Back:
			Montage = Back.Get();
			break;
		case EHitDirection::Left:
			Montage = Left.Get();
			break;
		case EHitDirection::Right:
			Montage = Right.Get();
			break;
		default:
			break;
		}

		return Montage ? Montage : Front.Get();
	}
};

/**
 * Plays an interrupting hit reaction. Started by UAbilityComponent when the
 * owner's combatant component reports a Stagger or stronger; never from input.
 * The owner has State.Staggered while it plays.
 */
UCLASS(Abstract, Blueprintable)
class ABILITYSYSTEM_API UHitReactionAbility : public UMontageAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility_Implementation() override;
	virtual void OnAbilityEnded_Implementation(EAbilityEndReason EndReason) override;

	UFUNCTION(BlueprintPure, Category = "Hit Reaction")
	const FHitReactionResult& GetHitReaction() const { return HitReaction; }
	
	UHitReactionAbility();

protected:
	/** Picks the montage for this reaction. Default: StaggerMontages by direction. */
	UFUNCTION(BlueprintNativeEvent, Category = "Hit Reaction")
	UAnimMontage* SelectReactionMontage(const FHitReactionResult& Reaction) const;
	virtual UAnimMontage* SelectReactionMontage_Implementation(const FHitReactionResult& Reaction) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction")
	FHitReactionMontages StaggerMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hit Reaction", meta = (ClampMin = "0.1"))
	float ReactionPlayRate = 1.0f;

private:
	UPROPERTY(Transient)
	FHitReactionResult HitReaction;

	bool bStaggerTagApplied = false;
};